# Windows 目标检测服务部署验证记录

验证日期：2026-06-15

## 1. 已完成内容

- 创建独立 `cloud_detection_server` Python 项目。
- 实现 `GET /health` 和 `POST /detect`。
- `/detect` 接收 JPEG、`frame_id` 和可选 `capture_ts_ms`。
- 服务启动时加载一次 `yolo11n.pt`，请求间复用模型。
- 实现文件大小、Content-Type、图片解码、帧编号和时间戳校验。
- 返回目标类别、置信度、原图坐标、图片尺寸、推理耗时和请求耗时。
- 提供 PowerShell 启动脚本、本机测试客户端、自动化测试和部署 README。

## 2. 实际环境

```text
Windows Python : 3.11.7
FastAPI        : 0.137.0
Uvicorn        : 0.49.0
Ultralytics    : 8.4.67
YOLO model     : yolo11n.pt
OpenCV         : 4.13.0.92
Pytest         : 9.1.0
```

模型文件已成功下载到服务端项目根目录，大小为 5,613,764 bytes。

## 3. 实际执行结果

### 自动化测试

执行：

```powershell
.\.venv\Scripts\python.exe -m pytest -q
```

结果：

```text
7 passed
```

覆盖：

- `/health` 返回模型状态。
- 有效 JPEG 返回检测结果并保持 `frame_id`。
- 非图片内容返回 400。
- 非 JPEG Content-Type 返回 400。
- 缺少文件返回统一 422。
- 超过大小限制返回 413。
- 负数 `frame_id` 返回 400。

### 服务监听

启动日志：

```text
model_loaded model=yolo11n.pt
Uvicorn running on http://0.0.0.0:8080
```

本机和局域网地址健康检查均成功：

```text
http://127.0.0.1:8080/health
http://192.168.9.110:8080/health
```

真实响应：

```json
{"status":"ok","model_loaded":true,"model_name":"yolo11n.pt"}
```

### 真实目标检测

使用项目已有的 `video2lcd/capture.jpg`，图片可被 OpenCV 解码为 `800x600` JPEG。

执行：

```powershell
.\.venv\Scripts\python.exe .\scripts\test_client.py ..\video2lcd\capture.jpg --frame-id 101
```

真实响应：

```json
{
  "frame_id": 101,
  "capture_ts_ms": null,
  "image_width": 800,
  "image_height": 600,
  "inference_ms": 452.19,
  "request_ms": 456.961,
  "detections": [
    {
      "class_id": 0,
      "class_name": "person",
      "confidence": 0.6706363558769226,
      "x1": 0,
      "y1": 51,
      "x2": 429,
      "y2": 599
    }
  ]
}
```

## 4. 当前运行状态

服务当前以以下地址运行：

```text
0.0.0.0:8080
```

本次检测到的可用 Windows 局域网地址之一：

```text
192.168.9.110
```

实际板端应选择与 i.MX6ULL 位于同一网段的 Windows 地址。

## 5. 板端待验证命令

在 i.MX6ULL 上执行：

```bash
ping 192.168.9.110
curl http://192.168.9.110:8080/health
curl -X POST \
  -F "file=@capture.jpg" \
  -F "frame_id=1" \
  http://192.168.9.110:8080/detect
```

板端联调尚未由本机验证。若访问失败，应先确认双方网络和 Windows 防火墙。

## 6. 尚未完成且不属于本次范围

- 未修改板端 C 上传代码。当前旧接口只提交 `file`，后续需要增加 `frame_id` 并将 URL 改为 `/detect`。
- 未验证 i.MX6ULL 到 Windows 的真实网络请求。
- 未实现视频流、数据库、Web 页面、认证、高并发或容器部署。
- 未进行模型训练、精度优化或 GPU 推理优化。
