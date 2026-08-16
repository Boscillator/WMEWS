import marimo

__generated_with = "0.23.16"
app = marimo.App(width="medium")


@app.cell
def _():
    import polars as pl
    import matplotlib.pyplot as plt

    return pl, plt


@app.cell
def _(pl):
    df = pl.read_parquet("../data/processed/*.parquet")
    df
    return (df,)


@app.cell
def _(df, pl, plt):
    real = df.filter(pl.col("real_cycle"))
    for run in df.partition_by('run_index'):
        included = run.filter(pl.col("included"))
        excluded = run.filter(pl.col("included").not_())
        positives = included.filter(pl.col("label"))
        negatives = included.filter(pl.col("label").not_())

        plt.plot(excluded['capture_first_sample_time'], excluded['covariance_trace_g2'], label="Excluded")
        plt.plot(positives['capture_first_sample_time'], positives['covariance_trace_g2'], label="Positives")
        plt.plot(negatives['capture_first_sample_time'], negatives['covariance_trace_g2'], label="Negatives")
        plt.ylim(0, df['covariance_trace_g2'].max())
        plt.legend()
        plt.grid()
        plt.show()
    return (real,)


@app.cell
def _(pl, real):
    real.filter(pl.col("label"))
    return


@app.cell
def _():
    return


if __name__ == "__main__":
    app.run()
