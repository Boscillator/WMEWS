"""Split feature Parquet files into stratified train, validation, and test datasets."""

import argparse
from dataclasses import dataclass
from pathlib import Path

import polars as pl

SPLIT_NAMES = ("train", "validation", "test")
REQUIRED_COLUMNS = {"run_index", "label"}


@dataclass(frozen=True)
class SplitPercentages:
    """Requested allocation percentage for each output split."""

    train: float
    validation: float
    test: float

    def values(self) -> tuple[float, float, float]:
        return (self.train, self.validation, self.test)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Split feature Parquet files into stratified train/validation/test sets.",
    )
    parser.add_argument(
        "input", type=Path, help="Directory containing input .parquet files"
    )
    parser.add_argument(
        "out",
        type=Path,
        help="Directory for train.parquet, validation.parquet, and test.parquet",
    )
    parser.add_argument(
        "--train-pct",
        type=float,
        default=75.0,
        help="Training percentage (default: 75)",
    )
    parser.add_argument(
        "--validation-pct",
        type=float,
        default=25.0,
        help="Validation percentage (default: 25)",
    )
    parser.add_argument(
        "--test-pct", type=float, default=0.0, help="Test percentage (default: 0)"
    )
    return parser.parse_args()


def validate_percentages(percentages: SplitPercentages) -> None:
    """Ensure split percentages form a non-negative 100 percent allocation."""
    if any(value < 0 for value in percentages.values()):
        raise ValueError("Split percentages must be non-negative")
    if abs(sum(percentages.values()) - 100.0) > 1e-9:
        raise ValueError("Split percentages must sum to 100")


def resolve_input_files(input_path: Path) -> list[Path]:
    """Return sorted Parquet files below an existing input directory."""
    if not input_path.is_dir():
        raise FileNotFoundError(f"Input path is not a directory: {input_path}")
    files = sorted(input_path.rglob("*.parquet"))
    if not files:
        raise FileNotFoundError(f"No .parquet files found under {input_path}")
    return files


def load_input_data(files: list[Path]) -> pl.DataFrame:
    """Read and concatenate feature files after validating their required columns."""
    data = pl.concat([pl.read_parquet(path) for path in files])
    missing = REQUIRED_COLUMNS - set(data.columns)
    if missing:
        raise ValueError(
            f"Input data is missing required columns: {', '.join(sorted(missing))}"
        )
    return data


def allocate_run_ids(
    run_ids: list[int], percentages: SplitPercentages
) -> dict[str, list[int]]:
    """Allocate one label stratum of sorted run IDs using largest-remainder rounding."""
    total = len(run_ids)
    targets = [total * percentage / 100.0 for percentage in percentages.values()]
    counts = [int(target) for target in targets]
    remainder = total - sum(counts)
    ranked_remainders = sorted(
        range(len(SPLIT_NAMES)),
        key=lambda index: (-(targets[index] - counts[index]), index),
    )
    for index in ranked_remainders[:remainder]:
        counts[index] += 1

    assignments: dict[str, list[int]] = {}
    offset = 0
    for name, count in zip(SPLIT_NAMES, counts, strict=True):
        assignments[name] = run_ids[offset : offset + count]
        offset += count
    return assignments


def split_run_ids(
    data: pl.DataFrame, percentages: SplitPercentages
) -> dict[str, list[int]]:
    """Stratify whole runs by whether they contain one or more positive samples."""
    run_labels = (
        data.group_by("run_index")
        .agg(pl.col("label").cast(pl.Boolean).any().alias("has_positive"))
        .sort("run_index")
    )
    assignments = {name: [] for name in SPLIT_NAMES}
    for has_positive in (True, False):
        run_ids = run_labels.filter(pl.col("has_positive") == has_positive)[
            "run_index"
        ].to_list()
        for name, ids in allocate_run_ids(run_ids, percentages).items():
            assignments[name].extend(ids)
    return assignments


def write_splits(
    data: pl.DataFrame, assignments: dict[str, list[int]], out: Path
) -> None:
    """Write each assigned collection of complete runs and report its composition."""
    out.mkdir(parents=True, exist_ok=True)
    for name in SPLIT_NAMES:
        split = data.filter(pl.col("run_index").is_in(assignments[name]))
        output_path = out / f"{name}.parquet"
        split.write_parquet(output_path)
        positive_runs = (
            split.group_by("run_index")
            .agg(pl.col("label").any().alias("positive"))
            .filter("positive")
            .height
        )
        print(
            f"{name}: wrote {split.height} rows from {len(assignments[name])} runs "
            f"({positive_runs} positive runs) to {output_path}"
        )


def main() -> None:
    args = parse_args()
    percentages = SplitPercentages(args.train_pct, args.validation_pct, args.test_pct)
    validate_percentages(percentages)
    data = load_input_data(resolve_input_files(args.input))
    write_splits(data, split_run_ids(data, percentages), args.out)


if __name__ == "__main__":
    main()
