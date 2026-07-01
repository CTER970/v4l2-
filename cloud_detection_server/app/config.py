import os
from dataclasses import dataclass


def _env_float(name: str, default: float) -> float:
    """读取浮点型环境变量；值非法时使用默认值，避免服务启动失败。"""
    try:
        return float(os.getenv(name, str(default)))
    except ValueError:
        return default


def _env_int(name: str, default: int) -> int:
    """读取整型环境变量；值非法时使用默认值。"""
    try:
        return int(os.getenv(name, str(default)))
    except ValueError:
        return default


@dataclass(frozen=True)
class Settings:
    """服务运行配置。

    使用不可变数据类，防止请求处理过程中意外修改全局配置。
    """

    host: str = "0.0.0.0"
    port: int = 8080
    model_path: str = "yolo11n.pt"
    confidence_threshold: float = 0.40
    max_upload_bytes: int = 10 * 1024 * 1024

    @classmethod
    def from_env(cls) -> "Settings":
        """从环境变量构造配置，并为未配置项提供可直接运行的默认值。"""
        return cls(
            host=os.getenv("DETECT_HOST", "0.0.0.0"),
            port=_env_int("DETECT_PORT", 8080),
            model_path=os.getenv("MODEL_PATH", "yolo11n.pt"),
            confidence_threshold=_env_float("CONFIDENCE_THRESHOLD", 0.40),
            max_upload_bytes=_env_int("MAX_UPLOAD_BYTES", 10 * 1024 * 1024),
        )
