# WMEWS Ingest API

FastAPI endpoint for issuing authenticated, 15-minute S3 presigned PUT URLs for
raw washing-machine captures.

## Setup

From this directory, install the runtime and development dependencies:

```bash
uv sync --group dev
```

The application requires these environment variables. The bucket must exist and
the process needs AWS credentials that can write objects to it.

```bash
export WMEWS_API_SECRET="replace-with-a-long-random-secret"
export RAW_DATA_BUCKET="wmews-raw-data-example"
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

The tests mock S3 presigning and require neither AWS credentials nor network
access.

## Upload Contract

Request a URL with the device's shared secret and integer device ID:

```bash
curl -sS \
  -H "X-WMEWS-Secret: $WMEWS_API_SECRET" \
  -H "X-WMEWS-Device-Id: 42" \
  http://127.0.0.1:8000/upload-url
```

A successful response contains an `upload_url`, an S3 `object_key` in the form
`device_id=42/date=YYYY-MM-DD/<uuidv7>.washcap`, `method: "PUT"`, a required
`Content-Type: application/octet-stream` header, and an expiry of 900 seconds.
PUT the capture bytes to `upload_url` using the returned method and headers.

For a local manual contract check without AWS, replace `boto3.client` with a
stubbed presigner as in the test suite. A real local request needs usable AWS
credentials because S3 signing uses the normal boto3 credential chain.

## Lambda

The Terraform Lambda handler value is `main.handler`. It wraps the same
FastAPI application used locally through Mangum.
