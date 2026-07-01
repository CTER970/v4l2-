from time import perf_counter

import cv2
import numpy as np
import pytest
from fastapi.testclient import TestClient

from app.config import Settings
from app.detector import DetectionResult
from app.main import create_app
from app.schemas import Detection


class FakeDetector:
    """用于接口测试的假检测器，避免测试依赖真实 YOLO 模型和网络。"""

    model_name = "fake-detector"
    loaded = True
    load_error = None

    def load(self) -> None:
        self.loaded = True

    def detect(self, image: np.ndarray) -> DetectionResult:
        start = perf_counter()
        height, width = image.shape[:2]
        return DetectionResult(
            detections=[
                Detection(
                    class_id=0,
                    class_name="test_object",
                    confidence=0.9,
                    x1=0,
                    y1=0,
                    x2=width,
                    y2=height,
                )
            ],
            inference_ms=(perf_counter() - start) * 1000.0,
        )


@pytest.fixture
def jpeg_bytes() -> bytes:
    """在内存中生成一张可被 OpenCV 解码的 JPEG。"""
    image = np.zeros((48, 64, 3), dtype=np.uint8)
    ok, encoded = cv2.imencode(".jpg", image)
    assert ok
    return encoded.tobytes()


@pytest.fixture
def client():
    """创建注入假检测器的 FastAPI 测试客户端。"""
    settings = Settings(max_upload_bytes=1024 * 1024)
    with TestClient(create_app(settings=settings, detector=FakeDetector())) as test_client:
        yield test_client
