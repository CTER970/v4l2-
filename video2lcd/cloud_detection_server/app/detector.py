"""目标检测器封装。

设计要点:
1. 服务启动时加载一次模型(build_detector), 整个进程复用同一实例, 绝不每请求重载。
2. detect(image_bgr) 是同步推理接口, 返回 (detections, inference_ms, (w, h))。
3. 真实 YOLO(ultralytics)加载失败时, 按 ENABLE_FAKE_DETECTOR 决定降级方式:
   - 关闭(默认): 返回 UnavailableDetector(loaded=False), /detect 将回 503。
   - 开启:       返回 FakeDetector(loaded=True), 用于纯链路联调。
4. 坐标钳制到图片范围内; 检测结果按置信度从高到低排序。
"""
import time


class UnavailableDetector:
    """真实模型加载失败且未启用 fake 时的占位: loaded=False, 触发 /detect 返回 503。"""

    name = None
    loaded = False

    def detect(self, image_bgr, conf_threshold=0.4):
        raise RuntimeError("detector not loaded")


class FakeDetector:
    """假检测器: 返回空检测 + 模拟耗时, 让端云链路在没有真实模型时也能联调。"""

    def __init__(self):
        self.name = "fake-detector"
        self.loaded = True

    def detect(self, image_bgr, conf_threshold=0.4):
        h, w = image_bgr.shape[:2]
        start = time.perf_counter()
        time.sleep(0.005)  # 模拟一次推理耗时, 让 inference_ms 有合理数值
        inference_ms = (time.perf_counter() - start) * 1000.0
        return [], inference_ms, (w, h)


class YoloDetector:
    """真实 Ultralytics YOLO 检测器。"""

    def __init__(self, model_path, conf_threshold):
        # 延迟导入: 让真实模型成为可选依赖, 加载失败不影响服务启动
        from ultralytics import YOLO

        self._model = YOLO(model_path)
        self._names = self._model.names  # {0: 'person', 1: 'bicycle', ...}
        self.name = model_path
        self.loaded = True

    def detect(self, image_bgr, conf_threshold=0.4):
        h, w = image_bgr.shape[:2]
        start = time.perf_counter()
        results = self._model.predict(image_bgr, conf=conf_threshold, verbose=False)
        inference_ms = (time.perf_counter() - start) * 1000.0

        detections = []
        for r in results:
            boxes = getattr(r, "boxes", None)
            if boxes is None:
                continue
            for b in boxes:
                x1, y1, x2, y2 = b.xyxy[0].tolist()
                cls_id = int(b.cls[0])
                conf = float(b.conf[0])
                detections.append({
                    "class_id": cls_id,
                    "class_name": self._names.get(cls_id, str(cls_id)),
                    "confidence": round(conf, 4),
                    # 坐标限制在图片范围内, 避免越界
                    "x1": max(0, int(round(x1))),
                    "y1": max(0, int(round(y1))),
                    "x2": min(w, int(round(x2))),
                    "y2": min(h, int(round(y2))),
                })
        # 检测结果按置信度从高到低排列
        detections.sort(key=lambda d: d["confidence"], reverse=True)
        return detections, inference_ms, (w, h)


def build_detector(model_path, conf_threshold, enable_fake=False):
    """启动时调用一次, 构造进程级单例检测器。"""
    try:
        det = YoloDetector(model_path, conf_threshold)
        print(f"[detector] real YOLO loaded: {model_path}")
        return det
    except Exception as e:
        print(f"[detector] YOLO load failed: {e}")
        if enable_fake:
            print("[detector] ENABLE_FAKE_DETECTOR=1, using fake-detector for link test")
            return FakeDetector()
        print("[detector] model unavailable; /detect will return 503")
        return UnavailableDetector()
