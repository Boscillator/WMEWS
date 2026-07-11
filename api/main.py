from fastapi import FastAPI
from mangum import Mangum
from pydantic import BaseModel


app = FastAPI(title="WMEWS Ingest API")


class UploadUrlResponse(BaseModel):
    """Placeholder payload for the future presigned-upload flow."""

    upload_url: str
    object_key: str
    expires_in_seconds: int


@app.get("/upload-url", response_model=UploadUrlResponse)
async def get_upload_url() -> UploadUrlResponse:
    """Return deterministic dummy data for the future presigned-upload flow."""
    return UploadUrlResponse(
        upload_url="https://example.invalid/wmews-uploads/placeholder.pb",
        object_key="wmews-uploads/placeholder.pb",
        expires_in_seconds=3600,
    )


# Lambda entrypoint; local ASGI servers use ``app`` directly.
handler = Mangum(app)
