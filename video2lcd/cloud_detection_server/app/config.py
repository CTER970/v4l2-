"""cloud_detection_server 配置。

所有配置项均可通过环境变量覆盖, 方便板端联调时按需调整。
默认监听 0.0.0.0:8080, 保证 i.MX6ULL 板端在同一局域网内可访问。
"""
import os

# 监听地址: 0.0.0.0 表示所有网卡, 板端才能访问; 不要改成 127.0.0.1
HOST = os.environ.get("DETECT_HOST", "0.0.0.0")
PORT = int(os.environ.get("DETECT_PORT", "8080"))

# Ultralytics YOLO 模型, 优先 yolo11n.pt; 当前版本不支持时可在环境变量换其他 nano 模型
MODEL_PATH = os.environ.get("MODEL_PATH", "yolo11n.pt")

# 置信度阈值: 低于此值的目标会被丢弃
CONFIDENCE_THRESHOLD = float(os.environ.get("CONFIDENCE_THRESHOLD", "0.4"))

# 上传文件大小上限, 默认 10MB
MAX_UPLOAD_BYTES = int(os.environ.get("MAX_UPLOAD_BYTES", str(10 * 1024 * 1024)))

# 真实 YOLO 加载失败时的兜底策略:
#   0 (默认): 返回不可用检测器, /health 的 model_loaded=false, /detect 返回 503。
#   1        : 启用假检测器, /detect 返回 200 + 空检测, 用于在无真实模型时验证端云收发链路。
# README 中已说明该开关的用途。
ENABLE_FAKE_DETECTOR = os.environ.get("ENABLE_FAKE_DETECTOR", "0") == "1"
