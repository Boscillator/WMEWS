"""Tests for the standalone train/validation/test split script."""

import importlib.util
from pathlib import Path

import polars as pl
import pytest

SCRIPT_PATH = Path(__file__).parents[1] / "test-train-split.py"
spec = importlib.util.spec_from_file_location("test_train_split", SCRIPT_PATH)
assert spec is not None and spec.loader is not None
splitter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(splitter)


def test_validate_percentages_rejects_invalid_allocations() -> None:
    with pytest.raises(ValueError, match="non-negative"):
        splitter.validate_percentages(splitter.SplitPercentages(-1, 101, 0))
    with pytest.raises(ValueError, match="sum to 100"):
        splitter.validate_percentages(splitter.SplitPercentages(70, 20, 0))


def test_resolve_input_files_finds_nested_parquet_files(tmp_path: Path) -> None:
    (tmp_path / "nested").mkdir()
    (tmp_path / "a.parquet").touch()
    (tmp_path / "nested" / "b.parquet").touch()

    assert splitter.resolve_input_files(tmp_path) == [
        tmp_path / "a.parquet",
        tmp_path / "nested" / "b.parquet",
    ]


def test_split_keeps_runs_whole_and_stratifies_labels(tmp_path: Path) -> None:
    data = pl.DataFrame(
        {
            "run_index": [1, 1, 2, 2, 3, 4, 5, 6],
            "label": [True, False, True, False, True, False, False, False],
            "feature": list(range(8)),
        }
    )
    data[:4].write_parquet(tmp_path / "first.parquet")
    data[4:].write_parquet(tmp_path / "second.parquet")

    loaded = splitter.load_input_data(splitter.resolve_input_files(tmp_path))
    assignments = splitter.split_run_ids(
        loaded, splitter.SplitPercentages(train=75, validation=25, test=0)
    )
    splitter.write_splits(loaded, assignments, tmp_path / "out")

    assert {run for ids in assignments.values() for run in ids} == {1, 2, 3, 4, 5, 6}
    assert not set(assignments["train"]) & set(assignments["validation"])
    assert assignments["test"] == []

    train = pl.read_parquet(tmp_path / "out" / "train.parquet")
    validation = pl.read_parquet(tmp_path / "out" / "validation.parquet")
    test = pl.read_parquet(tmp_path / "out" / "test.parquet")
    assert train["run_index"].n_unique() == 4
    assert validation["run_index"].n_unique() == 2
    assert test.height == 0
    for split in (train, validation):
        labels = split.group_by("run_index").agg(
            pl.col("label").any().alias("positive")
        )
        assert labels.filter("positive").height > 0
        assert labels.filter(~pl.col("positive")).height > 0


def test_load_input_data_requires_run_and_label_columns(tmp_path: Path) -> None:
    pl.DataFrame({"run_index": [1]}).write_parquet(tmp_path / "features.parquet")

    with pytest.raises(ValueError, match="label"):
        splitter.load_input_data(splitter.resolve_input_files(tmp_path))
