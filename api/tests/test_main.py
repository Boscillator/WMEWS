import pytest
from httpx import ASGITransport, AsyncClient

from main import app


@pytest.fixture
def anyio_backend() -> str:
    return "asyncio"


@pytest.mark.anyio
async def test_get_upload_url_returns_dummy_upload_data() -> None:
    async with AsyncClient(
        transport=ASGITransport(app=app), base_url="http://testserver"
    ) as client:
        response = await client.get("/upload-url")

    assert response.status_code == 200
    assert response.json() == {
        "upload_url": "https://example.invalid/wmews-uploads/placeholder.pb",
        "object_key": "wmews-uploads/placeholder.pb",
        "expires_in_seconds": 3600,
    }
