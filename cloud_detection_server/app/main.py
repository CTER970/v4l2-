import logging
import time
from contextlib import asynccontextmanager

import cv2
import numpy as np
from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from .config import Settings
from .detector import Detector, YoloDetector
from .schemas import DetectResponse, ErrorResponse, HealthResponse

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
)
logger = logging.getLogger("detection_server")


def _error(status_code: int, error: str, message: str) -> HTTPException:
    """构造统一错误结构，保持 HTTP 状态码和业务错误码分离。"""
    return HTTPException(
        status_code=status_code,
        detail={"error": error, "message": message},
    )


def create_app(
    settings: Settings | None = None,
    detector: Detector | None = None,
) -> FastAPI:
    """创建 FastAPI 应用。

    settings 和 detector 均支持依赖注入，生产环境使用真实 YOLO，
    自动化测试则注入假检测器，保证测试快速且不依赖模型文件。
    """
    current_settings = settings or Settings.from_env()
    current_detector = detector or YoloDetector(
        current_settings.model_path,
        current_settings.confidence_threshold,
    )

    @asynccontextmanager
    async def lifespan(_: FastAPI):
        # 模型只在服务启动阶段加载一次，所有检测请求复用同一实例。
        if not current_detector.loaded:
            current_detector.load()
        if current_detector.loaded:
            logger.info("model_loaded model=%s", current_detector.model_name)
        else:
            logger.error(
                "model_load_failed model=%s error=%s",
                current_detector.model_name,
                current_detector.load_error,
            )
        yield

    app = FastAPI(
        title="video2lcd 目标检测服务端",
        description="接收板端上传的 JPEG 图片，并返回目标检测结果。",
        version="1.0.0",
        lifespan=lifespan,
    )
    app.state.settings = current_settings
    app.state.detector = current_detector

    @app.exception_handler(HTTPException)
    async def http_exception_handler(_, exc: HTTPException):
        # 主动抛出的业务异常直接保持统一 JSON 格式。
        if isinstance(exc.detail, dict) and "error" in exc.detail:
            return JSONResponse(status_code=exc.status_code, content=exc.detail)
        return JSONResponse(
            status_code=exc.status_code,
            content={"error": "http_error", "message": str(exc.detail)},
        )

    @app.exception_handler(RequestValidationError)
    async def validation_exception_handler(_, exc: RequestValidationError):
        # FastAPI 默认校验错误格式较复杂，这里统一成 error/message。
        return JSONResponse(
            status_code=422,
            content={"error": "validation_error", "message": str(exc.errors())},
        )

    @app.get("/health", response_model=HealthResponse)
    async def health() -> HealthResponse:
        """返回服务状态；模型加载失败时服务仍可启动并报告 degraded。"""
        return HealthResponse(
            status="ok" if current_detector.loaded else "degraded",
            model_loaded=current_detector.loaded,
            model_name=current_detector.model_name,
        )

    @app.post(
        "/detect",
        response_model=DetectResponse,
        responses={
            400: {"model": ErrorResponse},
            413: {"model": ErrorResponse},
            500: {"model": ErrorResponse},
            503: {"model": ErrorResponse},
        },
    )
    async def detect(
        file: UploadFile = File(...),
        frame_id: int = Form(...),
        capture_ts_ms: int | None = Form(None),
    ) -> DetectResponse:
        """校验并解码一张 JPEG，执行目标检测后返回标准 JSON。"""
        request_start = time.perf_counter()
        server_receive_ts_ms = int(time.time() * 1000)

        if frame_id < 0:
            raise _error(400, "invalid_frame_id", "frame_id 必须为非负整数")
        if capture_ts_ms is not None and capture_ts_ms < 0:
            raise _error(
                400,
                "invalid_capture_ts",
                "capture_ts_ms 必须为非负整数",
            )
        if not current_detector.loaded:
            raise _error(503, "model_not_loaded", "目标检测模型当前不可用")

        # Content-Type 是第一层快速检查，后续仍会使用 OpenCV 验证真实内容。
        content_type = (file.content_type or "").lower()
        if content_type not in {"image/jpeg", "image/jpg", "image/pjpeg"}:
            raise _error(400, "invalid_content_type", "上传文件必须为 JPEG")

        # 分块读取并在读取过程中限制大小，避免超大文件占用过多内存。
        chunks: list[bytes] = []
        total_bytes = 0
        while chunk := await file.read(1024 * 1024):
            total_bytes += len(chunk)
            if total_bytes > current_settings.max_upload_bytes:
                raise _error(413, "file_too_large", "上传文件超过大小限制")
            chunks.append(chunk)

        if total_bytes == 0:
            raise _error(400, "invalid_image", "上传文件为空")

        # 不信任扩展名和 Content-Type，必须确认图片能够被真实解码。
        encoded = np.frombuffer(b"".join(chunks), dtype=np.uint8)
        image = cv2.imdecode(encoded, cv2.IMREAD_COLOR)
        if image is None:
            raise _error(400, "invalid_image", "上传文件无法解码为有效图片")

        image_height, image_width = image.shape[:2]
        try:
            result = current_detector.detect(image)
        except Exception as exc:
            logger.exception("detect_failed frame_id=%s", frame_id)
            raise _error(500, "inference_failed", str(exc)) from exc

        request_ms = (time.perf_counter() - request_start) * 1000.0
        logger.info(
            "detect_ok frame_id=%s size=%sx%s bytes=%s detections=%s "
            "inference_ms=%.2f request_ms=%.2f",
            frame_id,
            image_width,
            image_height,
            total_bytes,
            len(result.detections),
            result.inference_ms,
            request_ms,
        )
        return DetectResponse(
            frame_id=frame_id,
            capture_ts_ms=capture_ts_ms,
            server_receive_ts_ms=server_receive_ts_ms,
            inference_ms=round(result.inference_ms, 3),
            request_ms=round(request_ms, 3),
            image_width=image_width,
            image_height=image_height,
            detections=result.detections,
        )

    return app


app = create_app()
