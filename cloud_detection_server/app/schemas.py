from pydantic import BaseModel, Field


class Detection(BaseModel):
    """单个目标的类别、置信度和原图像素坐标。"""

    class_id: int
    class_name: str
    confidence: float = Field(ge=0.0, le=1.0)
    x1: int
    y1: int
    x2: int
    y2: int


class DetectResponse(BaseModel):
    """一次目标检测请求的完整响应。"""

    frame_id: int
    capture_ts_ms: int | None
    server_receive_ts_ms: int
    inference_ms: float
    request_ms: float
    image_width: int
    image_height: int
    detections: list[Detection]


class HealthResponse(BaseModel):
    """健康检查响应，用于确认服务和模型是否可用。"""

    status: str
    model_loaded: bool
    model_name: str


class ErrorResponse(BaseModel):
    """统一错误响应；error 用于程序判断，message 用于人工阅读。"""

    error: str
    message: str
