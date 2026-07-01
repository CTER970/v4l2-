from dataclasses import dataclass
from pathlib import Path
from time import perf_counter
from typing import Protocol

import numpy as np

from .schemas import Detection


@dataclass
class DetectionResult:
    """检测器内部返回值，将检测列表与纯推理耗时绑定。"""

    detections: list[Detection]
    inference_ms: float


class Detector(Protocol):
    """检测器接口。

    Web 层只依赖该接口，因此测试时可以注入假检测器，避免加载真实模型。
    """

    model_name: str
    loaded: bool
    load_error: str | None

    def load(self) -> None: ...

    def detect(self, image: np.ndarray) -> DetectionResult: ...


class YoloDetector:
    """Ultralytics YOLO 的轻量封装，负责模型生命周期和结果格式转换。"""

    def __init__(self, model_path: str, confidence_threshold: float):
        self.model_path = model_path
        self.model_name = Path(model_path).name
        self.confidence_threshold = confidence_threshold
        self.loaded = False
        self.load_error: str | None = None
        self._model = None

    def load(self) -> None:
        """加载一次模型，并记录失败原因供健康检查和日志使用。"""
        try:
            from ultralytics import YOLO

            self._model = YOLO(self.model_path)
            self.loaded = True
            self.load_error = None
        except Exception as exc:
            self.loaded = False
            self.load_error = str(exc)

    def detect(self, image: np.ndarray) -> DetectionResult:
        """执行 CPU 推理，并将 Ultralytics 结果转换为稳定的项目协议。"""
        if not self.loaded or self._model is None:
            raise RuntimeError(self.load_error or "目标检测模型尚未加载")

        image_height, image_width = image.shape[:2]
        # perf_counter 使用单调时钟，适合测量纯模型推理耗时。
        start = perf_counter()
        results = self._model.predict(
            source=image,
            conf=self.confidence_threshold,
            verbose=False,
            device="cpu",
        )
        inference_ms = (perf_counter() - start) * 1000.0

        detections: list[Detection] = []
        for result in results:
            names = result.names
            for box in result.boxes:
                x1, y1, x2, y2 = box.xyxy[0].tolist()
                class_id = int(box.cls[0].item())
                confidence = float(box.conf[0].item())
                # 驱动端只需要原图像素坐标，因此将坐标限制在图片边界内。
                detections.append(
                    Detection(
                        class_id=class_id,
                        class_name=str(names[class_id]),
                        confidence=max(0.0, min(1.0, confidence)),
                        x1=max(0, min(image_width, round(x1))),
                        y1=max(0, min(image_height, round(y1))),
                        x2=max(0, min(image_width, round(x2))),
                        y2=max(0, min(image_height, round(y2))),
                    )
                )

        # 按置信度降序返回，便于板端优先处理最可信目标。
        detections.sort(key=lambda item: item.confidence, reverse=True)
        return DetectionResult(detections=detections, inference_ms=inference_ms)
