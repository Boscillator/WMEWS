# syntax=docker/dockerfile:1
FROM astral/uv:python3.12-alpine AS builder

ENV UV_COMPILE_BYTECODE=1 \
    UV_LINK_MODE=copy

WORKDIR /opt/wmews-cli

RUN apk add --no-cache build-base gfortran linux-headers openblas-dev

COPY pyproject.toml uv.lock ./
RUN uv sync --locked --no-dev --no-install-project

COPY . ./
RUN uv sync --locked --no-dev

FROM python:3.12-alpine

WORKDIR /opt/wmews-cli

RUN apk add --no-cache libgfortran libstdc++ openblas

COPY --from=builder /opt/wmews-cli /opt/wmews-cli

ENV PATH="/opt/wmews-cli/.venv/bin:$PATH" \
    PYTHONUNBUFFERED=1

# SageMaker pipeline steps override this with a script such as
# extract-features.py or test-train-split.py and supply their input/output paths.
ENTRYPOINT ["python"]
