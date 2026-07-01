"""cloud_detection_server FastAPI 应用。

接口:
  GET  /health  -> 服务与模型状态
  POST /detect  -> 接收 multipart/form-data (file, frame_id, capture_ts_ms?)
                   返回检测 JSON

错误处理统一返回 {"error": <code>, "message": <str>}, 状态码:
  400 invalid_image / invalid_frame_id / invalid_capture_ts_ms
  413 file_too_large
  422 validation_error (缺少必填字段)
  500 inference_error
  503 model_not_loaded
"""
import time
from contextlib import asynccontextmanager
from typing import Optional

import cv2
import numpy as np
from fastapi import FastAPI, File, Form, UploadFile, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from .config import (
    HOST, PORT, MODEL_PATH,
    CONFIDENCE_THRESHOLD, MAX_UPLOAD_BYTES, ENABLE_FAKE_DETECTOR,
)
from .detector import build_detector


@asynccontextmanager
async def lifespan(app: FastAPI):
    # 服务启动时加载一次模型, 整个进程复用同一实例
    app.state.detector = build_detector(
        MODEL_PATH, CONFIDENCE_THRESHOLD, ENABLE_FAKE_DETECTOR
    )
    yield


app = FastAPI(title="cloud_detection_server", lifespan=lifespan)


def err(error_code: str, message: str, status: int) -> JSONResponse:
    """统一错误响应格式。"""
    return JSONResponse(
        status_code=status,
        content={"error": error_code, "message": message},
    )


@app.exception_handler(RequestValidationError)
async def validation_exception_handler(request: Request, exc: RequestValidationError):
    # 缺少必填字段(file / frame_id)时, FastAPI 默认返回 422, 这里统一成 {error, message}
    return JSONResponse(
        status_code=422,
        content={"error": "validation_error", "message": "missing or invalid form fields"},
    )


@app.get("/health")
async def health():
    detector = app.state.detector
    return {
        "status": "ok",
        "model_loaded": bool(getattr(detector, "loaded", False)),
        "model_name": getattr(detector, "name", None) or MODEL_PATH,
    }


@app.post("/detect")
async def detect(
    request: Request,
    file: UploadFile = File(...),
    frame_id: str = Form(...),
    capture_ts_ms: Optional[str] = Form(None),
):
    request_start = time.perf_counter()
    server_receive_ts_ms = int(time.time() * 1000)

    # 1. 校验 frame_id: 缺失由 FastAPI 触发 422; 非整数或负数返回 400
    try:
        frame_id_int = int(frame_id)
    except (TypeError, ValueError):
        return err("invalid_frame_id", "frame_id must be a non-negative integer", 400)
    if frame_id_int < 0:
        return err("invalid_frame_id", "frame_id must be a non-negative integer", 400)

    # 2. 校验 capture_ts_ms (可选): 提供则必须是非负整数
    capture_ts_int = None
    if capture_ts_ms not in (None, ""):
        try:
            capture_ts_int = int(capture_ts_ms)
        except (TypeError, ValueError):
            return err("invalid_capture_ts_ms", "capture_ts_ms must be a non-negative integer", 400)
        if capture_ts_int < 0:
            return err("invalid_capture_ts_ms", "capture_ts_ms must be a non-negative integer", 400)

    # 3. 读取并校验上传文件大小
    content = await file.read()
    if not content:
        return err("invalid_image", "uploaded file is empty", 400)
    if len(content) > MAX_UPLOAD_BYTES:
        return err("file_too_large", f"file exceeds {MAX_UPLOAD_BYTES} bytes", 413)

    # 4. JPEG 校验: 不能只信 Content-Type, 用 SOI 标记 + OpenCV 解码双重验证
    if not content.startswith(b"\xff\xd8"):
        return err("invalid_image", "uploaded file is not a JPEG (missing SOI marker)", 400)
    image_bgr = cv2.imdecode(np.frombuffer(content, np.uint8), cv2.IMREAD_COLOR)
    if image_bgr is None:
        return err("invalid_image", "uploaded file cannot be decoded as an image", 400)

    # 5. 模型可用性检查
    detector = app.state.detector
    if not getattr(detector, "loaded", False):
        return err("model_not_loaded", "model is not loaded", 503)

    # 6. 推理
    try:
        detections, inference_ms, (w, h) = detector.detect(image_bgr, CONFIDENCE_THRESHOLD)
    except Exception as e:
        return err("inference_error", f"inference failed: {e}", 500)

    request_ms = (time.perf_counter() - request_start) * 1000.0
    # 每次请求至少记录: frame_id、宽高、字节数、目标数、推理耗时、请求耗时、状态
    print(
        f"[detect] frame_id={frame_id_int} {w}x{h} bytes={len(content)} "
        f"objs={len(detections)} inference_ms={inference_ms:.1f} "
        f"request_ms={request_ms:.1f} ok"
    )

    resp = {"frame_id": frame_id_int}
    if capture_ts_int is not None:
        resp["capture_ts_ms"] = capture_ts_int
    resp.update({
        "server_receive_ts_ms": server_receive_ts_ms,
        "inference_ms": round(inference_ms, 2),
        "image_width": w,
        "image_height": h,
        "detections": detections,
    })
    return resp


if __name__ == "__main__":
    # 直接 python app/main.py 也可启动; 正式启动用 scripts/start_server.ps1
    import uvicorn
    uvicorn.run("app.main:app", host=HOST, port=PORT)
