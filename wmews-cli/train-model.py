"""Train and MLflow-autolog a balanced Random Forest classifier from feature splits."""

import argparse
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import mlflow
import mlflow.sklearn
import polars as pl
from numpy.typing import NDArray
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (
    ConfusionMatrixDisplay,
    PrecisionRecallDisplay,
    RocCurveDisplay,
    average_precision_score,
    precision_recall_fscore_support,
    roc_auc_score,
)

REQUIRED_COLUMNS = {"included", "label"}
EXCLUDED_FEATURE_COLUMNS = {
    "label",
    "included",
    "run_index",
    "capture_id",
    "capture_first_sample_time",
    "capture_last_sample_time",
    "real_cycle",
    "time_until_unbalanced",
}
type MaxFeatures = str | int | float | None


@dataclass(frozen=True)
class TrainingConfig:
    """Random Forest hyperparameters accepted by the training CLI."""

    n_estimators: int
    max_depth: int | None
    min_samples_split: int
    min_samples_leaf: int
    max_features: MaxFeatures
    random_state: int


@dataclass(frozen=True)
class ValidationEvaluation:
    """Metrics and predictions used to log validation results."""

    metrics: dict[str, float]
    probabilities: NDArray
    predictions: NDArray


def positive_int(value: str) -> int:
    """Parse an integer that must be greater than zero."""
    integer = int(value)
    if integer <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return integer


def probability(value: str) -> float:
    """Parse a probability including both endpoints."""
    result = float(value)
    if not 0 <= result <= 1:
        raise argparse.ArgumentTypeError("must be between 0 and 1")
    return result


def parse_max_features(value: str) -> MaxFeatures:
    """Parse scikit-learn's max_features values from a command-line argument."""
    if value == "none":
        return None
    if value in {"sqrt", "log2"}:
        return value
    try:
        if "." in value:
            fraction = float(value)
            if not 0 < fraction <= 1:
                raise ValueError
            return fraction
        return positive_int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "must be 'sqrt', 'log2', 'none', a positive integer, or a fraction in (0, 1]"
        ) from error


def parse_args() -> argparse.Namespace:
    """Parse training paths, tracking configuration, and model hyperparameters."""
    parser = argparse.ArgumentParser(
        description="Train and MLflow-autolog a Random Forest classifier.",
    )
    parser.add_argument("train", type=Path, help="Training split Parquet path")
    parser.add_argument("validation", type=Path, help="Validation split Parquet path")
    parser.add_argument(
        "--tracking-uri", required=True, help="MLflow tracking server URI or ARN"
    )
    parser.add_argument(
        "--registered-model-name",
        required=True,
        help="MLflow model-registry name for the trained model",
    )
    parser.add_argument("--n-estimators", type=positive_int, default=100)
    parser.add_argument("--max-depth", type=positive_int)
    parser.add_argument("--min-samples-split", type=positive_int, default=2)
    parser.add_argument("--min-samples-leaf", type=positive_int, default=1)
    parser.add_argument("--max-features", type=parse_max_features, default="sqrt")
    parser.add_argument("--random-state", type=int, default=42)
    parser.add_argument("--threshold", type=probability, default=0.5)
    return parser.parse_args()


def load_split(path: Path) -> pl.DataFrame:
    """Read an included feature split and validate its target columns."""
    if not path.is_file():
        raise FileNotFoundError(f"Split path is not a file: {path}")
    data = pl.read_parquet(path)
    missing = REQUIRED_COLUMNS - set(data.columns)
    if missing:
        raise ValueError(
            f"Split is missing required columns: {', '.join(sorted(missing))}"
        )
    if data["included"].dtype != pl.Boolean:
        raise ValueError("Split column 'included' must be Boolean")
    if data["included"].null_count() > 0:
        raise ValueError("Split column 'included' must not contain nulls")

    data = data.filter("included")
    if data.is_empty():
        raise ValueError(f"Split has no included rows: {path}")
    if data["label"].null_count() > 0:
        raise ValueError("Split column 'label' must not contain nulls")
    return data


def feature_columns(data: pl.DataFrame) -> list[str]:
    """Return predictive numeric and Boolean columns from an extracted feature split."""
    return [
        name
        for name, dtype in data.schema.items()
        if name not in EXCLUDED_FEATURE_COLUMNS
        and (dtype.is_numeric() or dtype == pl.Boolean)
    ]


def select_features(
    train: pl.DataFrame, validation: pl.DataFrame
) -> tuple[pl.DataFrame, pl.Series, pl.DataFrame, pl.Series, list[str]]:
    """Select schema-matched, non-null predictive features and binary training labels."""
    train_columns = feature_columns(train)
    validation_columns = feature_columns(validation)
    if train_columns != validation_columns:
        raise ValueError("Training and validation feature columns do not match")
    if not train_columns:
        raise ValueError("Split has no usable feature columns")

    for split_name, data in (("Training", train), ("Validation", validation)):
        null_columns = [name for name in train_columns if data[name].null_count() > 0]
        if null_columns:
            raise ValueError(
                f"{split_name} features contain nulls: {', '.join(null_columns)}"
            )

    train_labels = train["label"]
    validation_labels = validation["label"]
    if not train_labels.is_in([False, True]).all() or train_labels.n_unique() != 2:
        raise ValueError("Training labels must contain both Boolean classes")
    if not validation_labels.is_in([False, True]).all():
        raise ValueError("Validation labels must be Boolean")
    return (
        train.select(train_columns),
        train_labels,
        validation.select(train_columns),
        validation_labels,
        train_columns,
    )


def train_model(
    features: pl.DataFrame, labels: pl.Series, config: TrainingConfig
) -> RandomForestClassifier:
    """Fit the configured balanced Random Forest classifier."""
    model = RandomForestClassifier(
        n_estimators=config.n_estimators,
        max_depth=config.max_depth,
        min_samples_split=config.min_samples_split,
        min_samples_leaf=config.min_samples_leaf,
        max_features=config.max_features,
        class_weight="balanced",
        random_state=config.random_state,
    )
    model.fit(features, labels)
    return model


def evaluate_model(
    model: RandomForestClassifier,
    features: pl.DataFrame,
    labels: pl.Series,
    threshold: float,
) -> ValidationEvaluation:
    """Return validation metrics and predictions for a decision threshold."""
    probabilities = model.predict_proba(features)[:, 1]
    predictions = probabilities >= threshold
    precision, recall, f1, _ = precision_recall_fscore_support(
        labels.to_numpy(), predictions, average="binary", zero_division=0
    )
    metrics = {
        "validation_precision": float(precision),
        "validation_recall": float(recall),
        "validation_f1": float(f1),
    }
    if labels.n_unique() == 2:
        metrics["validation_roc_auc"] = float(roc_auc_score(labels, probabilities))
        metrics["validation_average_precision"] = float(
            average_precision_score(labels, probabilities)
        )
    return ValidationEvaluation(metrics, probabilities, predictions)


def log_validation_plots(labels: pl.Series, evaluation: ValidationEvaluation) -> None:
    """Log confusion-matrix, precision-recall, and ROC validation artifacts."""
    labels_array = labels.to_numpy()
    plots = [
        (
            "validation/confusion_matrix.png",
            lambda ax: ConfusionMatrixDisplay.from_predictions(
                labels_array, evaluation.predictions, ax=ax
            ),
        ),
        (
            "validation/precision_recall_curve.png",
            lambda ax: PrecisionRecallDisplay.from_predictions(
                labels_array, evaluation.probabilities, ax=ax
            ),
        ),
    ]
    if labels.n_unique() == 2:
        plots.append(
            (
                "validation/roc_curve.png",
                lambda ax: RocCurveDisplay.from_predictions(
                    labels_array, evaluation.probabilities, ax=ax
                ),
            )
        )
    for artifact_path, plot in plots:
        figure, axes = plt.subplots()
        plot(axes)
        mlflow.log_figure(figure, artifact_path)
        plt.close(figure)


def main() -> None:
    """Train a model and record it and its parameters in MLflow."""
    args = parse_args()
    config = TrainingConfig(
        n_estimators=args.n_estimators,
        max_depth=args.max_depth,
        min_samples_split=args.min_samples_split,
        min_samples_leaf=args.min_samples_leaf,
        max_features=args.max_features,
        random_state=args.random_state,
    )
    train = load_split(args.train)
    validation = load_split(args.validation)
    train_features, train_labels, validation_features, validation_labels, columns = (
        select_features(train, validation)
    )

    mlflow.set_tracking_uri(args.tracking_uri)
    mlflow.sklearn.autolog(
        log_models=True, registered_model_name=args.registered_model_name
    )
    with mlflow.start_run():
        mlflow.log_params(
            {
                "training_rows": train.height,
                "validation_rows": validation.height,
                "feature_count": len(columns),
                "feature_columns": ",".join(columns),
                "threshold": args.threshold,
            }
        )
        model = train_model(train_features, train_labels, config)
        evaluation = evaluate_model(
            model, validation_features, validation_labels, args.threshold
        )
        mlflow.log_metrics(evaluation.metrics)
        log_validation_plots(validation_labels, evaluation)

    print("Validation metrics:")
    for name, value in evaluation.metrics.items():
        print(f"{name}: {value:.4f}")

    importances = pl.DataFrame(
        {"feature": columns, "importance": model.feature_importances_}
    ).sort("importance", descending=True)
    print(f"Trained on {train.height} rows with {len(columns)} features.")
    print(importances)


if __name__ == "__main__":
    main()
