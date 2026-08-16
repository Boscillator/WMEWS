import marimo

__generated_with = "0.23.16"
app = marimo.App(width="medium")


@app.cell
def _():
    import polars as pl

    return (pl,)


@app.cell
def _(pl):
    df = pl.read_parquet("../data/processed/*.parquet")
    df
    return (df,)


@app.cell
def _(df):
    df.plot.scatter('capture_first_sample_time', 'covariance_trace_g2')
    return


@app.cell
def _():
    return


if __name__ == "__main__":
    app.run()
