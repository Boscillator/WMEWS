import re
from datetime import UTC, datetime
from unittest.mock import Mock

import pytest
from httpx import ASGITransport, AsyncClient

import main
from main import (
    API_SECRET_ENV,
    PRESIGNED_URL_EXPIRY_SECONDS,
    RAW_DATA_BUCKET_ENV,
    UPLOAD_CONTENT_TYPE,
    app,
)


@pytest.fixture
def anyio_backend() -> str:
    return "asyncio"


@pytest.mark.anyio
async def test_get_upload_url_returns_signed_put_contract(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    presigner = Mock(return_value="https://s3.example.test/signed-upload")
    s3_client = Mock(generate_presigned_url=presigner)
    monkeypatch.setenv(API_SECRET_ENV, "test-secret")
    monkeypatch.setenv(RAW_DATA_BUCKET_ENV, "test-raw-data")
    monkeypatch.setattr(main.boto3, "client", Mock(return_value=s3_client))

    async with AsyncClient(
        transport=ASGITransport(app=app), base_url="http://testserver"
    ) as client:
        response = await client.get(
            "/upload-url",
            headers={"X-WMEWS-Secret": "test-secret", "X-WMEWS-Device-Id": "42"},
        )

    assert response.status_code == 200
    body = response.json()
    assert body["upload_url"] == "https://s3.example.test/signed-upload"
    assert body["method"] == "PUT"
    assert body["headers"] == {"Content-Type": UPLOAD_CONTENT_TYPE}
    assert body["expires_in_seconds"] == PRESIGNED_URL_EXPIRY_SECONDS
    assert re.fullmatch(
        rf"device_id=42/date={datetime.now(UTC):%Y-%m-%d}/"
        r"[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\.washcap",
        body["object_key"],
    )
    presigner.assert_called_once_with(
        ClientMethod="put_object",
        Params={
            "Bucket": "test-raw-data",
            "Key": body["object_key"],
            "ContentType": UPLOAD_CONTENT_TYPE,
        },
        ExpiresIn=PRESIGNED_URL_EXPIRY_SECONDS,
        HttpMethod="PUT",
    )


@pytest.mark.anyio
@pytest.mark.parametrize(
    ("headers", "status_code"),
    [
        ({"X-WMEWS-Device-Id": "42"}, 401),
        ({"X-WMEWS-Secret": "wrong", "X-WMEWS-Device-Id": "42"}, 401),
        ({"X-WMEWS-Secret": "test-secret"}, 422),
        ({"X-WMEWS-Secret": "test-secret", "X-WMEWS-Device-Id": "not-an-int"}, 422),
    ],
)
async def test_get_upload_url_rejects_invalid_headers_without_presigning(
    monkeypatch: pytest.MonkeyPatch, headers: dict[str, str], status_code: int
) -> None:
    s3_client = Mock()
    monkeypatch.setenv(API_SECRET_ENV, "test-secret")
    monkeypatch.setenv(RAW_DATA_BUCKET_ENV, "test-raw-data")
    monkeypatch.setattr(main.boto3, "client", Mock(return_value=s3_client))

    async with AsyncClient(
        transport=ASGITransport(app=app), base_url="http://testserver"
    ) as client:
        response = await client.get("/upload-url", headers=headers)

    assert response.status_code == status_code
    s3_client.generate_presigned_url.assert_not_called()


@pytest.mark.anyio
@pytest.mark.parametrize("missing_environment", [API_SECRET_ENV, RAW_DATA_BUCKET_ENV])
async def test_get_upload_url_requires_server_configuration(
    monkeypatch: pytest.MonkeyPatch, missing_environment: str
) -> None:
    s3_client = Mock()
    monkeypatch.setenv(API_SECRET_ENV, "test-secret")
    monkeypatch.setenv(RAW_DATA_BUCKET_ENV, "test-raw-data")
    monkeypatch.delenv(missing_environment)
    monkeypatch.setattr(main.boto3, "client", Mock(return_value=s3_client))

    async with AsyncClient(
        transport=ASGITransport(app=app), base_url="http://testserver"
    ) as client:
        response = await client.get(
            "/upload-url",
            headers={"X-WMEWS-Secret": "test-secret", "X-WMEWS-Device-Id": "42"},
        )

    assert response.status_code == 500
    s3_client.generate_presigned_url.assert_not_called()
