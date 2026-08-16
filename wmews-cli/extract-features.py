"""Convert raw Washcap accelerometer samples to engineering units.

Reads input file by file and yields one DataFrame per "run" (a period of
continuous data with no gap > 30s). Each .washcap file is parsed natively by
Polars before its captures are yielded.
"""

import argparse
from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

import polars as pl

# A gap strictly larger than this between consecutive samples starts a new run.
RUN_GAP = timedelta(seconds=30)

OUTPUT_SCHEMA: dict[str, Any] = {
    "capture_id": pl.UInt32,
    "t": pl.Datetime("us", time_zone="UTC"),
    "sensor_tick": pl.Int64,
    "x_g": pl.Float64,
    "y_g": pl.Float64,
    "z_g": pl.Float64,
    "button_pressed": pl.Boolean,
    "firmware_version": pl.String,
    "sample_rate_hz": pl.Int64,
    "capture_start_time": pl.Datetime("us", time_zone="UTC"),
    "battery_millivolts": pl.Int64,
    "battery_read_ok": pl.Boolean,
    "device_target": pl.String,
    "device_model": pl.Int64,
    "device_revision": pl.Int64,
    "device_base_mac": pl.String,
    "accelerometer_range_g": pl.Int64,
    "lsb_per_g": pl.Int64,
}

# Parse only fields needed to decode samples or produce the output. Supplying a
# schema also prevents Polars from spending time inferring unused header fields.
_RAW_TIME_SCHEMA = pl.Struct(
    {"tick_duration_us": pl.Float64, "wrap_ticks": pl.Int64}
)
_RAW_AXIS_SCHEMA = pl.Struct(
    {"accelerometer_range_g": pl.Int64, "lsb_per_g": pl.Int64}
)
WASHCAP_SCHEMA: dict[str, Any] = {
    "start_time": pl.String,
    "button_pressed_at": pl.String,
    "firmware_version": pl.String,
    "sample_rate_hz": pl.Int64,
    "data_format": pl.Struct(
        {
            "raw": pl.Struct(
                {
                    "t": _RAW_TIME_SCHEMA,
                    "x": _RAW_AXIS_SCHEMA,
                    "y": _RAW_AXIS_SCHEMA,
                    "z": _RAW_AXIS_SCHEMA,
                }
            )
        }
    ),
    "battery": pl.Struct({"millivolts": pl.Int64, "read_ok": pl.Boolean}),
    "device": pl.Struct(
        {
            "target": pl.String,
            "model": pl.Int64,
            "revision": pl.Int64,
            "base_mac": pl.String,
        }
    ),
    "t": pl.Int64,
    "x": pl.Int64,
    "y": pl.Int64,
    "z": pl.Int64,
}


@dataclass(frozen=True)
class CaptureMetadata:
    """Metadata from a Washcap header needed to decode its samples.

    Timestamps are kept as raw strings; polars converts them when the
    capture frame is built.
    """

    start_time: str
    button_pressed_at: str | None
    tick_duration_us: float
    wrap_ticks: int
    x_lsb_per_g: int
    y_lsb_per_g: int
    z_lsb_per_g: int
    firmware_version: str | None
    sample_rate_hz: int | None
    battery_millivolts: int | None
    battery_read_ok: bool | None
    device_target: str | None
    device_model: int | None
    device_revision: int | None
    device_base_mac: str | None
    accelerometer_range_g: int | None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert .washcap samples to engineering units.",
    )
    parser.add_argument("input", type=Path, help="A .washcap file or directory")
    return parser.parse_args()


def require_mapping(value: Any, field_name: str) -> dict[str, Any]:
    """Return a header struct, raising a useful error for malformed input."""
    if not isinstance(value, dict):
        raise TypeError(
            f"Header field {field_name!r} must be a struct, not {type(value).__name__}",
        )
    return value


def require_string(value: Any, field_name: str) -> str:
    """Return a header string, raising a useful error for malformed input."""
    if not isinstance(value, str):
        raise TypeError(
            f"Header field {field_name!r} must be a string, not {type(value).__name__}",
        )
    return value


def parse_capture_metadata(row: dict[str, Any]) -> CaptureMetadata:
    """Extract and validate a capture header."""
    data_format = require_mapping(row.get("data_format"), "data_format")
    raw_format = require_mapping(data_format.get("raw"), "data_format.raw")
    time_format = require_mapping(raw_format.get("t"), "data_format.raw.t")
    x_format = require_mapping(raw_format.get("x"), "data_format.raw.x")
    y_format = require_mapping(raw_format.get("y"), "data_format.raw.y")
    z_format = require_mapping(raw_format.get("z"), "data_format.raw.z")

    start_time = require_string(row["start_time"], "start_time")
    button_pressed_at = row.get("button_pressed_at")
    if button_pressed_at is not None:
        button_pressed_at = require_string(button_pressed_at, "button_pressed_at")

    tick_duration_us = float(time_format["tick_duration_us"])
    wrap_ticks = int(time_format["wrap_ticks"])
    lsb_per_g = tuple(int(axis["lsb_per_g"]) for axis in (x_format, y_format, z_format))
    if tick_duration_us <= 0 or wrap_ticks <= 0 or min(lsb_per_g) <= 0:
        raise ValueError("Capture header has invalid timing or scale metadata")

    battery = row.get("battery") or {}
    device = row.get("device") or {}
    return CaptureMetadata(
        start_time=start_time,
        button_pressed_at=button_pressed_at,
        tick_duration_us=tick_duration_us,
        wrap_ticks=wrap_ticks,
        x_lsb_per_g=lsb_per_g[0],
        y_lsb_per_g=lsb_per_g[1],
        z_lsb_per_g=lsb_per_g[2],
        firmware_version=row.get("firmware_version"),
        sample_rate_hz=row.get("sample_rate_hz"),
        battery_millivolts=battery.get("millivolts"),
        battery_read_ok=battery.get("read_ok"),
        device_target=device.get("target"),
        device_model=device.get("model"),
        device_revision=device.get("revision"),
        device_base_mac=device.get("base_mac"),
        accelerometer_range_g=x_format.get("accelerometer_range_g"),
    )


def decode_capture(
    header: dict[str, Any], samples: pl.DataFrame, capture_id: int
) -> pl.DataFrame:
    """Decode one capture's raw samples into the output schema using polars."""
    meta = parse_capture_metadata(header)
    if not samples["t"].is_between(0, meta.wrap_ticks - 1).all():
        raise ValueError(
            f"Capture {capture_id} has ticks outside [0, {meta.wrap_ticks})"
        )

    # Elapsed ticks since capture start, accounting for tick wraparound.
    elapsed = (pl.col("t") - pl.col("t").shift(1)).fill_null(0) % meta.wrap_ticks
    frame = samples.with_columns(
        elapsed.cum_sum().alias("elapsed"),
        pl.col("t").cast(pl.Int64).alias("sensor_tick"),
        pl.lit(capture_id, pl.UInt32).alias("capture_id"),
        pl.lit(meta.start_time, pl.String).alias("capture_start_time"),
        pl.lit(meta.firmware_version, pl.String).alias("firmware_version"),
        pl.lit(meta.sample_rate_hz, pl.Int64).alias("sample_rate_hz"),
        pl.lit(meta.battery_millivolts, pl.Int64).alias("battery_millivolts"),
        pl.lit(meta.battery_read_ok, pl.Boolean).alias("battery_read_ok"),
        pl.lit(meta.device_target, pl.String).alias("device_target"),
        pl.lit(meta.device_model, pl.Int64).alias("device_model"),
        pl.lit(meta.device_revision, pl.Int64).alias("device_revision"),
        pl.lit(meta.device_base_mac, pl.String).alias("device_base_mac"),
        pl.lit(meta.accelerometer_range_g, pl.Int64).alias("accelerometer_range_g"),
        pl.lit(meta.x_lsb_per_g, pl.Int64).alias("lsb_per_g"),
        pl.lit(False, pl.Boolean).alias("button_pressed"),
    )
    frame = frame.with_columns(
        pl.col("capture_start_time").str.to_datetime(time_unit="us", time_zone="UTC")
    )
    frame = frame.with_columns(
        (
            pl.col("capture_start_time")
            + pl.duration(
                microseconds=(pl.col("elapsed") * meta.tick_duration_us).cast(pl.Int64)
            )
        ).alias("t"),
        (pl.col("x") / meta.x_lsb_per_g).alias("x_g"),
        (pl.col("y") / meta.y_lsb_per_g).alias("y_g"),
        (pl.col("z") / meta.z_lsb_per_g).alias("z_g"),
    )

    # Mark the sample nearest the button press; ties select the earlier sample.
    if meta.button_pressed_at is not None:
        press_at = pl.select(
            pl.lit(meta.button_pressed_at).str.to_datetime(
                time_unit="us", time_zone="UTC"
            )
        ).item()
        press_idx = (frame["t"] - press_at).abs().arg_min()
        frame = frame.with_columns(
            pl.when(pl.int_range(frame.height) == press_idx)
            .then(True)
            .otherwise(False)
            .alias("button_pressed")
        )

    return frame.drop(["x", "y", "z", "elapsed"]).select(list(OUTPUT_SCHEMA))


def iter_capture_frames(path: Path, first_capture_id: int) -> Iterator[pl.DataFrame]:
    """Decode one .washcap file, yielding one DataFrame per capture.

    Polars reads one file at a time; each yielded frame is independent of the
    file handle and prior captures.
    """
    records = pl.read_ndjson(path, schema=WASHCAP_SCHEMA)
    records = records.with_columns(
        pl.col("start_time").is_not_null().cast(pl.UInt32).cum_sum().alias("_capture")
    )
    headers = {
        row["_capture"]: row
        for row in records.filter(pl.col("start_time").is_not_null()).iter_rows(named=True)
    }
    samples = records.drop_nulls(["t", "x", "y", "z"])
    capture_id = first_capture_id

    for capture in samples.partition_by("_capture", maintain_order=True):
        capture_number = capture["_capture"][0]
        header = headers.get(capture_number)
        if header is None:
            raise ValueError(f"{path.name} contains sample data before a header")
        yield decode_capture(header, capture.drop("_capture"), capture_id)
        capture_id += 1


def resolve_input_files(input_path: Path) -> list[Path]:
    """Resolve the input path to a sorted list of .washcap files."""
    if input_path.is_file():
        if input_path.suffix != ".washcap":
            raise ValueError(f"Input file must have a .washcap suffix: {input_path}")
        return [input_path]
    if input_path.is_dir():
        files = sorted(input_path.glob("**/*.washcap"))
        if not files:
            raise FileNotFoundError(f"No .washcap files found under {input_path}")
        return files
    raise FileNotFoundError(f"Input path does not exist: {input_path}")


def load_enriched_runs(input_path: Path) -> Iterator[pl.DataFrame]:
    """Yield one DataFrame per run, streaming so memory stays bounded.

    A run is a maximal sequence of samples where consecutive samples are
    <= 30s apart; a gap > 30s starts a new run. Runs may span multiple
    captures and files. Files are processed in sorted order and captures
    within a file in file order, so input is assumed to be chronological.

    Each yielded DataFrame is independent; once the consumer moves to the
    next run, the previous one is eligible for garbage collection.
    """
    current_run: list[pl.DataFrame] = []
    last_t: datetime | None = None
    capture_id = 0

    for path in resolve_input_files(input_path):
        for capture in iter_capture_frames(path, capture_id + 1):
            capture_id += 1
            first_t = capture["t"][0]
            if last_t is not None and first_t - last_t > RUN_GAP:
                yield pl.concat(current_run)
                current_run = []
            current_run.append(capture)
            last_t = capture["t"][-1]

    if current_run:
        yield pl.concat(current_run)


def main() -> None:
    args = parse_args()
    for run_index, run in enumerate(load_enriched_runs(args.input), start=1):
        times = run["t"]
        print(
            f"Run {run_index}: {len(run)} samples: \n {repr(run)}"
        )


if __name__ == "__main__":
    main()
