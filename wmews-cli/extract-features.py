"""Convert raw Washcap accelerometer samples to engineering units."""

import argparse
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

import polars as pl

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


@dataclass(frozen=True)
class CaptureMetadata:
    """Metadata from a Washcap header needed to decode its samples."""

    start_time: datetime
    button_pressed_at: datetime | None
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


def require_datetime(value: Any, field_name: str) -> datetime:
    """Return a timestamp parsed by Polars from a Washcap header."""
    if not isinstance(value, datetime):
        raise TypeError(
            f"Header field {field_name!r} must be a timestamp, not {type(value).__name__}",
        )
    return value


def require_mapping(value: Any, field_name: str) -> dict[str, Any]:
    """Return a header struct, raising a useful error for malformed input."""
    if not isinstance(value, dict):
        raise TypeError(
            f"Header field {field_name!r} must be a struct, not {type(value).__name__}",
        )
    return value


def parse_capture_metadata(row: dict[str, Any], row_number: int) -> CaptureMetadata:
    """Extract and validate a capture header."""
    data_format = require_mapping(row.get("data_format"), "data_format")
    raw_format = require_mapping(data_format.get("raw"), "data_format.raw")
    time_format = require_mapping(raw_format.get("t"), "data_format.raw.t")
    x_format = require_mapping(raw_format.get("x"), "data_format.raw.x")
    y_format = require_mapping(raw_format.get("y"), "data_format.raw.y")
    z_format = require_mapping(raw_format.get("z"), "data_format.raw.z")

    start_time = require_datetime(row["start_time"], "start_time")
    button_pressed_at = row.get("button_pressed_at")
    if button_pressed_at is not None:
        button_pressed_at = require_datetime(button_pressed_at, "button_pressed_at")

    tick_duration_us = float(time_format["tick_duration_us"]) 
    wrap_ticks = int(time_format["wrap_ticks"])
    lsb_per_g = tuple(int(axis["lsb_per_g"]) for axis in (x_format, y_format, z_format))
    if tick_duration_us <= 0 or wrap_ticks <= 0 or min(lsb_per_g) <= 0:
        raise ValueError(f"Row {row_number} header has invalid timing or scale metadata")

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


def mark_button_press(rows: list[dict[str, Any]], pressed_at: datetime | None) -> None:
    """Mark the sample nearest the button press; ties select the earlier sample."""
    if pressed_at is not None and rows:
        index = min(range(len(rows)), key=lambda i: (abs(rows[i]["t"] - pressed_at), rows[i]["t"]))
        rows[index]["button_pressed"] = True


def normalize_and_expand_washcap(raw_data: pl.DataFrame) -> pl.DataFrame:
    """Expand Washcap records into timestamped accelerometer samples in g."""
    required_columns = {"start_time", "data_format", "t", "x", "y", "z"}
    missing_columns = required_columns - set(raw_data.columns)
    if missing_columns:
        missing = ", ".join(sorted(missing_columns))
        raise ValueError(f"Washcap data is missing required columns: {missing}")

    output_rows: list[dict[str, Any]] = []
    capture_rows: list[dict[str, Any]] = []
    metadata: CaptureMetadata | None = None
    previous_tick: int | None = None
    elapsed_ticks = 0
    capture_id = 0

    def finish_capture() -> None:
        nonlocal capture_rows
        if metadata is not None:
            mark_button_press(capture_rows, metadata.button_pressed_at)
        output_rows.extend(capture_rows)
        capture_rows = []

    for row_number, row in enumerate(raw_data.iter_rows(named=True)):
        if row["start_time"] is not None:
            finish_capture()
            metadata = parse_capture_metadata(row, row_number)
            capture_id += 1
            previous_tick = None
            elapsed_ticks = 0
            continue

        if not all(row[name] is not None for name in ("t", "x", "y", "z")):
            continue
        if metadata is None:
            raise ValueError(f"Row {row_number} contains sample data before a header")

        tick = int(row["t"])
        if not 0 <= tick < metadata.wrap_ticks:
            raise ValueError(f"Row {row_number} tick {tick} is outside [0, {metadata.wrap_ticks})")
        if previous_tick is not None:
            elapsed_ticks += (tick - previous_tick) % metadata.wrap_ticks
        previous_tick = tick

        timestamp = metadata.start_time + timedelta(
            microseconds=elapsed_ticks * metadata.tick_duration_us,
        )
        capture_rows.append(
            {
                "capture_id": capture_id,
                "t": timestamp,
                "sensor_tick": tick,
                "x_g": int(row["x"]) / metadata.x_lsb_per_g,
                "y_g": int(row["y"]) / metadata.y_lsb_per_g,
                "z_g": int(row["z"]) / metadata.z_lsb_per_g,
                "button_pressed": False,
                "firmware_version": metadata.firmware_version,
                "sample_rate_hz": metadata.sample_rate_hz,
                "capture_start_time": metadata.start_time,
                "battery_millivolts": metadata.battery_millivolts,
                "battery_read_ok": metadata.battery_read_ok,
                "device_target": metadata.device_target,
                "device_model": metadata.device_model,
                "device_revision": metadata.device_revision,
                "device_base_mac": metadata.device_base_mac,
                "accelerometer_range_g": metadata.accelerometer_range_g,
                "lsb_per_g": metadata.x_lsb_per_g,
            },
        )

    finish_capture()
    return pl.from_dicts(output_rows, schema=OUTPUT_SCHEMA, strict=True)


def load_washcap(input_path: Path) -> pl.DataFrame:
    """Load and parse Washcap NDJSON records from an input file or directory."""
    if input_path.is_file():
        if input_path.suffix != ".washcap":
            raise ValueError(f"Input file must have a .washcap suffix: {input_path}")
        source = input_path
    elif input_path.is_dir():
        source = input_path / "**" / "*.washcap"
    else:
        raise FileNotFoundError(f"Input path does not exist: {input_path}")

    raw_data = pl.read_ndjson(source, infer_schema_length=None, ignore_errors=True)
    timestamp_columns = ("start_time", "button_pressed_at")
    return raw_data.with_columns(
        pl.col(column).str.to_datetime(time_unit="us", time_zone="UTC")
        for column in timestamp_columns
        if raw_data.schema[column] == pl.String
    )


def main() -> None:
    args = parse_args()
    samples = normalize_and_expand_washcap(load_washcap(args.input))
    print(samples)


if __name__ == "__main__":
    main()
