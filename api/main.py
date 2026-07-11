import hmac
import os
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Literal

import boto3
from fastapi import FastAPI, Header, HTTPException
from mangum import Mangum
from pydantic import BaseModel
from uuid6 import uuid7


app = FastAPI(title="WMEWS Ingest API")

API_SECRET_ENV = "WMEWS_API_SECRET"
RAW_DATA_BUCKET_ENV = "RAW_DATA_BUCKET"
PRESIGNED_URL_EXPIRY_SECONDS = 900
UPLOAD_CONTENT_TYPE = "application/octet-stream"


class UploadUrlResponse(BaseModel):
    """Contract a device uses to upload one raw capture directly to S3."""

    upload_url: str
    object_key: str
    method: Literal["PUT"]
    headers: dict[str, str]
    expires_in_seconds: int


@dataclass(frozen=True)
class UploadConfiguration:
    """Server-owned settings required to issue an upload URL."""

    api_secret: str
    bucket: str


def get_required_configuration() -> UploadConfiguration:
    """Load deployment configuration without falling back to unsafe defaults."""
    api_secret = os.getenv(API_SECRET_ENV)
    bucket = os.getenv(RAW_DATA_BUCKET_ENV)
    if not api_secret or not bucket:
        raise HTTPException(status_code=500, detail="Server upload configuration is unavailable.")
    return UploadConfiguration(api_secret=api_secret, bucket=bucket)


def authenticate_device(
    x_wmews_secret: str | None, x_wmews_device_id: str | None
) -> tuple[int, str]:
    """Validate the device headers while keeping the configured secret private."""
    configuration = get_required_configuration()
    if x_wmews_secret is None or not hmac.compare_digest(
        x_wmews_secret, configuration.api_secret
    ):
        raise HTTPException(status_code=401, detail="Invalid device credentials.")
    if x_wmews_device_id is None:
        raise HTTPException(status_code=422, detail="X-WMEWS-Device-Id is required.")
    try:
        device_id = int(x_wmews_device_id)
    except ValueError as error:
        raise HTTPException(status_code=422, detail="X-WMEWS-Device-Id must be an integer.") from error
    return device_id, configuration.bucket


@app.get("/upload-url", response_model=UploadUrlResponse)
async def get_upload_url(
    x_wmews_secret: str | None = Header(default=None),
    x_wmews_device_id: str | None = Header(default=None),
) -> UploadUrlResponse:
    """Return a short-lived PUT URL for a server-generated capture object key."""
    device_id, bucket = authenticate_device(x_wmews_secret, x_wmews_device_id)
    date_partition = datetime.now(UTC).date().isoformat()
    object_key = f"device_id={device_id}/date={date_partition}/{uuid7()}.washcap"
    upload_url = boto3.client("s3").generate_presigned_url(
        ClientMethod="put_object",
        Params={
            "Bucket": bucket,
            "Key": object_key,
            "ContentType": UPLOAD_CONTENT_TYPE,
        },
        ExpiresIn=PRESIGNED_URL_EXPIRY_SECONDS,
        HttpMethod="PUT",
    )
    return UploadUrlResponse(
        upload_url=upload_url,
        object_key=object_key,
        method="PUT",
        headers={"Content-Type": UPLOAD_CONTENT_TYPE},
        expires_in_seconds=PRESIGNED_URL_EXPIRY_SECONDS,
    )


# Lambda entrypoint; local ASGI servers use ``app`` directly.
handler = Mangum(app)
