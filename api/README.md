# WMEWS Ingest API

Minimal FastAPI scaffold for the Washing Machine Early Warning System ingest
pipeline. `GET /upload-url` currently returns deterministic dummy upload data;
it does not call AWS or require configuration.

## Setup

From this directory, install the runtime and development dependencies:

```bash
uv sync --group dev
```

## Local development

Start the application with FastAPI's development CLI and reload enabled:

```bash
uv run fastapi dev main.py
```

Run the route-contract test with:

```bash
uv run pytest
```

## Lambda

The future Terraform Lambda handler value is `main.handler`. It wraps the same
FastAPI application used locally through Mangum.
