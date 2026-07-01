from fastapi.testclient import TestClient

from app.config import Settings
from app.main import create_app
from tests.conftest import FakeDetector


def test_valid_jpeg_returns_detection_and_preserves_frame_id(client, jpeg_bytes):
    response = client.post(
        "/detect",
        files={"file": ("test.jpg", jpeg_bytes, "image/jpeg")},
        data={"frame_id": "42", "capture_ts_ms": "123456"},
    )

    body = response.json()
    assert response.status_code == 200
    assert body["frame_id"] == 42
    assert body["capture_ts_ms"] == 123456
    assert body["image_width"] == 64
    assert body["image_height"] == 48
    assert body["detections"][0]["class_name"] == "test_object"
    assert "inference_ms" in body
    assert "request_ms" in body


def test_invalid_image_returns_400(client):
    response = client.post(
        "/detect",
        files={"file": ("bad.jpg", b"not an image", "image/jpeg")},
        data={"frame_id": "1"},
    )

    assert response.status_code == 400
    assert response.json()["error"] == "invalid_image"


def test_non_jpeg_content_type_returns_400(client):
    response = client.post(
        "/detect",
        files={"file": ("bad.txt", b"plain text", "text/plain")},
        data={"frame_id": "1"},
    )

    assert response.status_code == 400
    assert response.json()["error"] == "invalid_content_type"


def test_missing_file_returns_422(client):
    response = client.post("/detect", data={"frame_id": "1"})

    assert response.status_code == 422
    assert response.json()["error"] == "validation_error"


def test_oversized_file_returns_413(jpeg_bytes):
    settings = Settings(max_upload_bytes=8)
    with TestClient(create_app(settings=settings, detector=FakeDetector())) as client:
        response = client.post(
            "/detect",
            files={"file": ("large.jpg", jpeg_bytes, "image/jpeg")},
            data={"frame_id": "1"},
        )

    assert response.status_code == 413
    assert response.json()["error"] == "file_too_large"


def test_negative_frame_id_returns_400(client, jpeg_bytes):
    response = client.post(
        "/detect",
        files={"file": ("test.jpg", jpeg_bytes, "image/jpeg")},
        data={"frame_id": "-1"},
    )

    assert response.status_code == 400
    assert response.json()["error"] == "invalid_frame_id"
