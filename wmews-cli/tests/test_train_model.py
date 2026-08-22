"""Tests for the standalone model-training script."""

import importlib.util
from pathlib import Path

import polars as pl
import pytest

SCRIPT_PATH = Path(__file__).parents[1] / "train-model.py"
spec = importlib.util.spec_from_file_location("train_model", SCRIPT_PATH)
assert spec is not None and spec.loader is not None
trainer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trainer)


def split_frame() -> pl.DataFrame:
    """Create a minimal extracted feature split with metadata and predictors."""
    return pl.DataFrame(
        {
            "capture_id": [1, 2, 3, 4],
            "run_index": [1, 1, 2, 2],
            "included": [True, True, True, True],
            "label": [False, True, False, True],
            "real_cycle": [True, True, True, True],
            "time_until_unbalanced": [None, 1, None, 1],
            "x_g_min": [0.1, 0.2, 0.3, 0.4],
            "sample_count": [10, 11, 12, 13],
            "has_button_press": [False, True, False, True],
        }
    )


def test_select_features_uses_all_predictors_without_metadata() -> None:
    train_features, labels, validation_features, validation_labels, columns = (
        trainer.select_features(split_frame(), split_frame())
    )

    assert columns == ["x_g_min", "sample_count", "has_button_press"]
    assert train_features.columns == columns
    assert validation_features.columns == columns
    assert labels.to_list() == [False, True, False, True]
    assert validation_labels.to_list() == [False, True, False, True]


def test_select_features_rejects_mismatched_validation_schema() -> None:
    with pytest.raises(ValueError, match="do not match"):
        trainer.select_features(split_frame(), split_frame().drop("x_g_min"))


def test_load_split_filters_excluded_rows(tmp_path: Path) -> None:
    data = split_frame().with_columns(pl.Series("included", [True, False, True, True]))
    path = tmp_path / "train.parquet"
    data.write_parquet(path)

    loaded = trainer.load_split(path)

    assert loaded.height == 3


def test_train_model_fits_configured_classifier() -> None:
    features, labels, _, _, _ = trainer.select_features(split_frame(), split_frame())
    config = trainer.TrainingConfig(
        n_estimators=5,
        max_depth=2,
        min_samples_split=2,
        min_samples_leaf=1,
        max_features="sqrt",
        random_state=42,
    )

    model = trainer.train_model(features, labels, config)

    assert model.n_estimators == 5
    assert model.classes_.tolist() == [False, True]


def test_evaluate_model_returns_validation_metrics() -> None:
    features, labels, validation_features, validation_labels, _ = (
        trainer.select_features(split_frame(), split_frame())
    )
    model = trainer.train_model(
        features,
        labels,
        trainer.TrainingConfig(5, 2, 2, 1, "sqrt", 42),
    )

    evaluation = trainer.evaluate_model(
        model, validation_features, validation_labels, 0.5
    )

    assert set(evaluation.metrics) == {
        "validation_precision",
        "validation_recall",
        "validation_f1",
        "validation_roc_auc",
        "validation_average_precision",
    }
    assert len(evaluation.probabilities) == len(validation_labels)
    assert len(evaluation.predictions) == len(validation_labels)
