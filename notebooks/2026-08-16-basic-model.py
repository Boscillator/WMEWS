import marimo

__generated_with = "0.23.16"
app = marimo.App(width="medium")


@app.cell
def _():
    import polars as pl
    import sklearn
    import matplotlib.pyplot as plt

    from sklearn.ensemble import RandomForestClassifier
    from sklearn.metrics import RocCurveDisplay
    from sklearn.metrics import classification_report, confusion_matrix, roc_auc_score
    from sklearn.metrics import (
        PrecisionRecallDisplay,
        average_precision_score,
        ConfusionMatrixDisplay
    )
    from imblearn.over_sampling import RandomOverSampler



    return (
        ConfusionMatrixDisplay,
        PrecisionRecallDisplay,
        RandomForestClassifier,
        RandomOverSampler,
        average_precision_score,
        classification_report,
        confusion_matrix,
        pl,
        plt,
        roc_auc_score,
    )


@app.cell
def _(pl):
    train = pl.read_parquet("../data/split/train.parquet").filter(pl.col("included"))
    val = pl.read_parquet("../data/split/validation.parquet").filter(pl.col("included"))
    val
    return train, val


@app.cell
def _(pl, train, val):
    #FEATURE_COLUMNS = {"x_g_min", "x_g_max", "y_g_min", "y_g_max", "z_g_min", "z_g_max", "magnitude_g_min", "magnitude_g_max", "magnitude_g_std", "cov_xx_g2", "cov_yy_g2", "cov_zz_g2", "cov_xy_g2", "cov_xz_g2", "cov_yz_g2", "covariance_trace_g2", "x_g_dominant_frequency_hz", "y_g_dominant_frequency_hz", "z_g_dominant_frequency_hz", "x_g_spectral_entropy", "y_g_spectral_entropy", "z_g_spectral_entropy"}

    FEATURE_COLUMNS = {"cov_xx_g2", "cov_yy_g2", "cov_zz_g2", "cov_xy_g2", "cov_xz_g2", "cov_yz_g2"}

    def split_Xy(df: pl.DataFrame) -> tuple[pl.DataFrame, pl.DataFrame]:
        df = df.filter(pl.col("included"))
        X = df.select(*FEATURE_COLUMNS)
        y = df.select("label")
    
        return X,y

    X_train, y_train = split_Xy(train)
    X_val, y_val = split_Xy(val)
    y_train
    return X_train, X_val, y_train, y_val


@app.cell
def _(RandomOverSampler, X_train, y_train):
    sampler = RandomOverSampler(random_state=42)
    X_train_resampled, y_train_resampled = sampler.fit_resample(
        X_train.to_numpy(),
        y_train.to_numpy(),
    )
    y_train_resampled.sum(), len(y_train_resampled)
    return X_train_resampled, y_train_resampled


@app.cell
def _(RandomForestClassifier, X_train_resampled, y_train_resampled):
    model = RandomForestClassifier(
        class_weight="balanced",
        random_state=42,
    )
    model.fit(X_train_resampled, y_train_resampled)
    return (model,)


@app.cell
def _(
    X_train,
    X_val,
    classification_report,
    confusion_matrix,
    model,
    pl,
    roc_auc_score,
    y_val,
):
    y_pred = model.predict(X_val)
    y_prob = model.predict_proba(X_val)[:, 1]

    threshold = 0.5
    y_pred = y_prob >= threshold

    print(classification_report(y_val, y_pred))
    print(confusion_matrix(y_val, y_pred))
    print("ROC AUC:", roc_auc_score(y_val, y_prob))

    importance = pl.from_dict({
        "importance": model.feature_importances_,
        "column": X_train.columns,
    }).sort("importance")
    print(importance)
    return y_pred, y_prob


@app.cell
def _(PrecisionRecallDisplay, average_precision_score, plt, y_prob, y_val):
    PrecisionRecallDisplay.from_predictions(y_val, y_prob)

    print(
        "Average precision:",
        average_precision_score(y_val, y_prob)
    )
    plt.show()
    return


@app.cell
def _(ConfusionMatrixDisplay, plt, y_pred, y_val):
    ConfusionMatrixDisplay.from_predictions(y_val, y_pred, display_labels=["Balanced", "Unbalanced"])       
    plt.show()
    return


@app.cell
def _():
    return


if __name__ == "__main__":
    app.run()
