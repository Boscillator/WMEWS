import marimo

__generated_with = "0.23.16"
app = marimo.App(width="medium")


@app.cell
def _():
    import marimo as mo
    import polars as pl
    import numpy as np
    import matplotlib.pyplot as plt
    from datetime import datetime, timedelta, timezone
    from typing import Any

    import altair as alt
    alt.data_transformers.enable("vegafusion")
    return Any, datetime, np, pl, plt, timedelta, timezone


@app.cell
def _(pl):
    credentials = pl.CredentialProviderAWS(
        profile_name="default",
        region_name="us-east-1",
    )

    df_raw = pl.read_ndjson(
        "s3://wmews-raw-data-7fa80758/device_id=1/date=2026-08-16/*.washcap",
        credential_provider=credentials,
        infer_schema_length=None,
        ignore_errors=True
    )
    return (df_raw,)


@app.cell
def _(Any, datetime, df_raw, pl, timedelta, timezone):
    _OUTPUT_SCHEMA = {
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


    def _parse_utc_datetime(value: Any) -> datetime | None:
        if value is None:
            return None

        if isinstance(value, datetime):
            parsed = value
        elif isinstance(value, str):
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        else:
            raise TypeError(
                f"Expected datetime, ISO-8601 string, or None; got {type(value)!r}"
            )

        if parsed.tzinfo is None:
            return parsed.replace(tzinfo=timezone.utc)

        return parsed.astimezone(timezone.utc)


    def _require_mapping(
        value: Any,
        *,
        field_name: str,
    ) -> dict[str, Any]:
        if not isinstance(value, dict):
            raise ValueError(
                f"Header field {field_name!r} must be a struct/dict, "
                f"not {type(value).__name__}"
            )
        return value


    def _finalize_capture_button_press(
        capture_rows: list[dict[str, Any]],
        button_pressed_at: datetime | None,
    ) -> None:
        """
        Mark the sample closest to button_pressed_at.

        If button_pressed_at is None, all samples remain false. In an exact
        timestamp tie, the earlier sample is selected.
        """
        if button_pressed_at is None or not capture_rows:
            return

        closest_index = min(
            range(len(capture_rows)),
            key=lambda index: (
                abs(capture_rows[index]["t"] - button_pressed_at),
                capture_rows[index]["t"],
            ),
        )
        capture_rows[closest_index]["button_pressed"] = True


    def normalize_and_expand_washcap(df: pl.DataFrame) -> pl.DataFrame:
        """
        Expand Washcap records into one normalized row per sensor sample.

        A non-null start_time begins a capture. That header's metadata remains
        active until the next header or the end of the dataframe.

        Rows with complete t/x/y/z values are treated as data rows. All other
        non-header rows are ignored, including all-null footer rows.

        If button_pressed_at is present, the closest sample in that capture is
        marked button_pressed=True. Otherwise all samples are false.
        """
        required_columns = {
            "start_time",
            "firmware_version",
            "sample_rate_hz",
            "button_pressed_at",
            "data_format",
            "battery",
            "device",
            "t",
            "x",
            "y",
            "z",
        }

        missing_columns = required_columns - set(df.columns)
        if missing_columns:
            raise ValueError(
                "Washcap dataframe is missing required columns: "
                + ", ".join(sorted(missing_columns))
            )

        output_rows: list[dict[str, Any]] = []
        capture_rows: list[dict[str, Any]] = []

        header: dict[str, Any] | None = None
        capture_id = 0

        capture_start: datetime | None = None
        button_pressed_at: datetime | None = None
        tick_duration_us: float | None = None
        wrap_ticks: int | None = None

        previous_tick: int | None = None
        elapsed_ticks = 0

        def finalize_current_capture() -> None:
            nonlocal capture_rows

            _finalize_capture_button_press(
                capture_rows,
                button_pressed_at,
            )
            output_rows.extend(capture_rows)
            capture_rows = []

        for row_number, row in enumerate(df.iter_rows(named=True)):
            is_header = row.get("start_time") is not None
            is_data = all(
                row.get(name) is not None
                for name in ("t", "x", "y", "z")
            )

            if is_header:
                # A new header implicitly closes the previous capture.
                if header is not None:
                    finalize_current_capture()

                data_format = _require_mapping(
                    row["data_format"],
                    field_name="data_format",
                )
                raw_format = _require_mapping(
                    data_format.get("raw"),
                    field_name="data_format.raw",
                )
                time_format = _require_mapping(
                    raw_format.get("t"),
                    field_name="data_format.raw.t",
                )

                capture_start = _parse_utc_datetime(row["start_time"])
                button_pressed_at = _parse_utc_datetime(
                    row.get("button_pressed_at")
                )
                tick_duration_us = float(time_format["tick_duration_us"])
                wrap_ticks = int(time_format["wrap_ticks"])

                if tick_duration_us <= 0:
                    raise ValueError(
                        f"Row {row_number} has non-positive tick_duration_us: "
                        f"{tick_duration_us}"
                    )

                if wrap_ticks <= 0:
                    raise ValueError(
                        f"Row {row_number} has non-positive wrap_ticks: "
                        f"{wrap_ticks}"
                    )

                header = row
                capture_id += 1
                previous_tick = None
                elapsed_ticks = 0
                continue

            if is_data:
                if (
                    header is None
                    or capture_start is None
                    or tick_duration_us is None
                    or wrap_ticks is None
                ):
                    raise ValueError(
                        f"Row {row_number} contains sample data before any header"
                    )

                current_tick = int(row["t"])

                if not 0 <= current_tick < wrap_ticks:
                    raise ValueError(
                        f"Row {row_number} has tick {current_tick}, outside "
                        f"[0, {wrap_ticks})"
                    )

                if previous_tick is None:
                    elapsed_ticks = 0
                else:
                    elapsed_ticks += (
                        current_tick - previous_tick
                    ) % wrap_ticks

                previous_tick = current_tick

                timestamp = capture_start + timedelta(
                    microseconds=elapsed_ticks * tick_duration_us
                )

                raw_format = header["data_format"]["raw"]
                x_lsb_per_g = int(raw_format["x"]["lsb_per_g"])
                y_lsb_per_g = int(raw_format["y"]["lsb_per_g"])
                z_lsb_per_g = int(raw_format["z"]["lsb_per_g"])

                if min(x_lsb_per_g, y_lsb_per_g, z_lsb_per_g) <= 0:
                    raise ValueError(
                        f"Row {row_number} has non-positive lsb_per_g metadata"
                    )

                battery = header.get("battery") or {}
                device = header.get("device") or {}

                capture_rows.append(
                    {
                        "capture_id": capture_id,
                        "t": timestamp,
                        "sensor_tick": current_tick,
                        "x_g": int(row["x"]) / x_lsb_per_g,
                        "y_g": int(row["y"]) / y_lsb_per_g,
                        "z_g": int(row["z"]) / z_lsb_per_g,
                        "button_pressed": False,
                        "firmware_version": header.get("firmware_version"),
                        "sample_rate_hz": header.get("sample_rate_hz"),
                        "capture_start_time": capture_start,
                        "battery_millivolts": battery.get("millivolts"),
                        "battery_read_ok": battery.get("read_ok"),
                        "device_target": device.get("target"),
                        "device_model": device.get("model"),
                        "device_revision": device.get("revision"),
                        "device_base_mac": device.get("base_mac"),
                        "accelerometer_range_g": raw_format["x"].get(
                            "accelerometer_range_g"
                        ),
                        "lsb_per_g": x_lsb_per_g,
                    }
                )
                continue

            # Ignore footer, blank, and otherwise non-sample rows.
            continue

        # The end of the dataframe implicitly closes the final capture.
        if header is not None:
            finalize_current_capture()

        return pl.from_dicts(
            output_rows,
            schema=_OUTPUT_SCHEMA,
            strict=True,
        )

    df = normalize_and_expand_washcap(df_raw)
    df
    return (df,)


@app.cell
def _(df, pl):
    agg = (
        df
        .sort("t")
        .group_by_dynamic(index_column="t", every="30s")
        .agg(
            pl.col("x_g").std(),
            pl.col("y_g").std(),
            pl.col("z_g").std(),
            pl.col("button_pressed").sum(),
        )
    )

    agg = agg.with_columns(
        (
            pl.col("x_g") ** 2
            + pl.col("y_g") ** 2
            + pl.col("z_g") ** 2
        ).sqrt().alias("g_magnitude")
    )

    agg
    return (agg,)


@app.cell
def _(agg, np, pl, plt, timedelta):
    segments = (
        agg
        .sort("t")
        .with_columns(
            (pl.col("t").diff() > pl.duration(seconds=30))
            .fill_null(False)
            .cum_sum()
            .alias("_segment")
        )
        .partition_by("_segment", include_key=False)
    )

    segments = [
        segment
        for segment in segments
        if segment["t"].max() - segment["t"].min()>= timedelta(minutes=20)
    ]

    for s in segments:
        plt.plot(s['t'], s['g_magnitude'])

        plt.yticks(np.arange(0, 0.1, 0.01))
        plt.xlabel("Time")
        plt.ylabel("Stdev Acceleration Magnitude (g)")
        plt.grid()

        if s['g_magnitude'].max() > 0.06:
            plt.title("Suspected Unbalanced")
        else:
            plt.title("Normal Run")

        plt.show()
    return


@app.cell
def _():
    return


if __name__ == "__main__":
    app.run()
