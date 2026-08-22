"""Tests for the standalone feature-extraction script."""

import importlib.util
import json
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Any

import polars as pl
import pytest

SCRIPT_PATH = Path(__file__).parents[1] / "extract-features.py"
spec = importlib.util.spec_from_file_location("extract_features", SCRIPT_PATH)
assert spec is not None and spec.loader is not None
extract_features = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract_features)


BASE_TIME = datetime(2026, 1, 1, tzinfo=UTC)


def header(
    start_time: datetime, *, button_pressed_at: datetime | None = None
) -> dict[str, Any]:
    """Build the minimum valid Washcap header."""
    axis = {"accelerometer_range_g": 16, "lsb_per_g": 1000}
    return {
        "start_time": start_time.isoformat().replace("+00:00", "Z"),
        "button_pressed_at": (
            button_pressed_at.isoformat().replace("+00:00", "Z")
            if button_pressed_at
            else None
        ),
        "firmware_version": "test",
        "sample_rate_hz": 100,
        "data_format": {
            "raw": {
                "t": {"tick_duration_us": 1_000_000.0, "wrap_ticks": 100},
                "x": axis,
                "y": axis,
                "z": axis,
            }
        },
        "battery": {"millivolts": 3700, "read_ok": True},
        "device": {"target": "test", "model": 1, "revision": 2, "base_mac": "aa"},
    }


def samples(*ticks: int) -> list[dict[str, int]]:
    return [
        {"t": tick, "x": index * 1000, "y": -index * 1000, "z": 1000}
        for index, tick in enumerate(ticks)
    ]


def write_washcap(path: Path, records: list[dict[str, Any]]) -> None:
    path.write_text("".join(f"{json.dumps(record)}\n" for record in records))


def raw_frame(
    capture_ids: list[int],
    timestamps: list[datetime],
    x_values: list[float],
    *,
    sample_rate_hz: int = 100,
) -> pl.DataFrame:
    return pl.DataFrame(
        {
            "capture_id": capture_ids,
            "t": timestamps,
            "sample_rate_hz": [sample_rate_hz] * len(capture_ids),
            "x_g": x_values,
            "y_g": [0.0] * len(capture_ids),
            "z_g": [0.0] * len(capture_ids),
        }
    )


def test_decode_capture_scales_wraps_and_marks_nearest_button_press() -> None:
    decoded = extract_features.decode_capture(
        header(BASE_TIME, button_pressed_at=BASE_TIME + timedelta(seconds=2.6)),
        pl.DataFrame(samples(98, 99, 1, 2)),
        capture_id=7,
    )

    assert decoded.schema == extract_features.OUTPUT_SCHEMA
    assert decoded["capture_id"].to_list() == [7, 7, 7, 7]
    assert decoded["sensor_tick"].to_list() == [98, 99, 1, 2]
    assert decoded["x_g"].to_list() == [0.0, 1.0, 2.0, 3.0]
    assert decoded["t"].to_list() == [
        BASE_TIME,
        BASE_TIME + timedelta(seconds=1),
        BASE_TIME + timedelta(seconds=3),
        BASE_TIME + timedelta(seconds=4),
    ]
    assert decoded["button_pressed"].to_list() == [False, False, True, False]


@pytest.mark.parametrize(
    ("input_path_kind", "expected_exception"),
    [("missing", FileNotFoundError), ("wrong-suffix", ValueError)],
)
def test_resolve_input_files_rejects_invalid_input(
    tmp_path: Path, input_path_kind: str, expected_exception: type[Exception]
) -> None:
    input_path = tmp_path / ("missing" if input_path_kind == "missing" else "data.txt")
    if input_path_kind == "wrong-suffix":
        input_path.touch()

    with pytest.raises(expected_exception):
        extract_features.resolve_input_files(input_path)


def test_load_enriched_runs_splits_gaps_and_preserves_sorted_file_order(
    tmp_path: Path,
) -> None:
    write_washcap(
        tmp_path / "b.washcap",
        [header(BASE_TIME + timedelta(seconds=40)), *samples(0, 1)],
    )
    write_washcap(
        tmp_path / "a.washcap",
        [header(BASE_TIME), *samples(0, 1)],
    )

    runs = list(extract_features.load_enriched_runs(tmp_path))

    assert len(runs) == 2
    assert runs[0]["capture_id"].unique().to_list() == [1]
    assert runs[1]["capture_id"].unique().to_list() == [2]
    assert runs[0]["t"].min() == BASE_TIME
    assert runs[1]["t"].min() == BASE_TIME + timedelta(seconds=40)


def test_summary_features_by_chunk_computes_statistics() -> None:
    data = pl.DataFrame(
        {
            "capture_id": [1, 1],
            "t": [BASE_TIME, BASE_TIME + timedelta(seconds=1)],
            "x_g": [0.0, 3.0],
            "y_g": [0.0, 4.0],
            "z_g": [0.0, 0.0],
        }
    )

    result = extract_features.summary_features_by_chunk(data).row(0, named=True)

    assert result["sample_count"] == 2
    assert result["x_g_min"] == 0.0
    assert result["y_g_max"] == 4.0
    assert result["magnitude_g_max"] == 5.0
    assert result["magnitude_g_mean"] == 2.5
    assert result["covariance_trace_g2"] == pytest.approx(12.5)


def test_dc_block_filter_validates_required_columns_and_resets_per_capture() -> None:
    with pytest.raises(ValueError, match="x_g"):
        extract_features.dc_block_filter_acceleration(pl.DataFrame({"capture_id": [1]}))

    data = raw_frame(
        [1, 1, 2, 2],
        [BASE_TIME + timedelta(seconds=index) for index in range(4)],
        [3.0, 3.0, 10.0, 10.0],
    )
    filtered = extract_features.dc_block_filter_acceleration(data)

    assert filtered["x_g"].abs().max() == pytest.approx(0.0, abs=1e-12)


def test_annotate_unbalanced_samples_labels_only_preceding_real_cycle_samples() -> None:
    features = pl.DataFrame(
        {
            "capture_first_sample_time": [
                BASE_TIME,
                BASE_TIME + timedelta(minutes=1),
                BASE_TIME + timedelta(minutes=2),
            ],
            "covariance_trace_g2": [0.001, 0.005, 0.006],
            "real_cycle": [True, True, True],
        }
    )

    result = extract_features.annotate_unbalanced_samples(features)

    assert result["label"].to_list() == [True, False, False]
    assert result["included"].to_list() == [True, False, False]
    assert result["time_until_unbalanced"].to_list() == [
        timedelta(minutes=1),
        None,
        None,
    ]


@pytest.mark.parametrize("real_cycle", [False, True])
def test_annotate_unbalanced_samples_keeps_balanced_runs(real_cycle: bool) -> None:
    features = pl.DataFrame(
        {
            "capture_first_sample_time": [BASE_TIME],
            "covariance_trace_g2": [extract_features.UNBALANCED_COVARIANCE_TRACE_G2],
            "real_cycle": [real_cycle],
        }
    )

    result = extract_features.annotate_unbalanced_samples(features)

    assert result["label"].to_list() == [False]
    assert result["included"].to_list() == [True]
    assert result["time_until_unbalanced"].to_list() == [None]
