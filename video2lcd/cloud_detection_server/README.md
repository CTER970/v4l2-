# cloud_detection_server

i.MX6ULL 嵌入式 Linux 相机项目的 **Windows 配套目标检测服务端**, 用于 P0/P1 联调。

只做一件事:**稳定提供 `/health` 和 `/detect`,让板端上传 JPEG 并收到 JSON 检测结果**。不做数据库、用户系统、前端页面、鉴权、HTTPS。

```text
i.MX6ULL 板端  --HTTP 上传 JPEG + frame_id-->  Windows 本服务  --JSON 检测结果-->  板端
```

技术栈:Windows 10/11 + Python 3.10/3.11 + FastAPI + Uvicorn + OpenCV + Ultralytics YOLO。

---

## 1. 目录结构

```text
cloud_detection_server/
├── app/
│   ├── __init__.py
│   ├── main.py          # FastAPI 应用: /health, /detect, 统一错误处理
│   ├── detector.py      # YOLO 单例 + fake + 不可用降级
│   └── config.py        # 配置 (可被环境变量覆盖)
├── scripts/
│   └── start_server.ps1 # PowerShell 启动脚本
├── samples/
│   └── README.md        # 测试图片说明
├── tests/
│   └── test_client.py   # 本机测试脚本 (requests, 也可 pytest)
├── requirements.txt
└── README.md
```

---

## 2. 创建虚拟环境并安装依赖

在 `cloud_detection_server` 目录下, 用 PowerShell 执行:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
```

> 如果激活脚本被策略禁止, 仅在当前用户范围放开 (不要改系统级策略):
> ```powershell
> Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
> ```

首次运行 `ultralytics` 时会自动下载 `yolo11n.pt` (约 5MB), 需要联网。

---

## 3. 启动服务

```powershell
.\scripts\start_server.ps1
```

等价命令 (在项目根目录、已激活 `.venv` 时):

```powershell
uvicorn app.main:app --host 0.0.0.0 --port 8080
```

默认监听 `0.0.0.0:8080` (所有网卡), 板端才能访问。看到下面这行说明就绪:

```text
INFO:     Uvicorn running on http://0.0.0.0:8080 (Press CTRL+C to quit)
[detector] real YOLO loaded: yolo11n.pt
```

---

## 4. Windows 本机测试

### 4.1 健康检查

```powershell
curl.exe http://127.0.0.1:8080/health
```

期望:

```json
{"status":"ok","model_loaded":true,"model_name":"yolo11n.pt"}
```

### 4.2 上传图片检测

先准备 `samples\test.jpg` (见 [samples/README.md](samples/README.md), 没有图片可用其中一行命令合成), 然后:

```powershell
curl.exe -X POST `
  -F "file=@samples/test.jpg" `
  -F "frame_id=1" `
  -F "capture_ts_ms=123456789" `
  http://127.0.0.1:8080/detect
```

### 4.3 自动化测试脚本

```powershell
python tests/test_client.py
```

该脚本依次测试 `/health`、`/detect`、缺少 `file` 时的 422。也可用 `pytest tests/` 运行。

---

## 5. i.MX6ULL 板端联调

### 5.1 找到 Windows 局域网 IP

在 Windows 上执行 `ipconfig`, 假设得到 `192.168.1.100` (替换为你自己的 IP)。

板端先验证连通性:

```bash
ping 192.168.1.100
curl http://192.168.1.100:8080/health
```

### 5.2 板端上传 capture.jpg

```bash
curl http://<Windows_IP>:8080/health

curl -X POST \
  -F "file=@capture.jpg;type=image/jpeg" \
  -F "frame_id=1" \
  -F "capture_ts_ms=123456789" \
  http://<Windows_IP>:8080/detect
```

板端收到检测 JSON 即完成基础端云链路。

---

## 6. Windows 防火墙放行 8080 端口

如果板端 `ping` 通但 `curl /health` 超时, 多半是 Windows 防火墙拦了入站。**以管理员身份**打开 PowerShell 执行 (本仓库不会自动执行):

```powershell
New-NetFirewallRule `
  -DisplayName "video2lcd detection server" `
  -Direction Inbound `
  -Protocol TCP `
  -LocalPort 8080 `
  -Action Allow
```

---

## 7. Fake Detector 模式 (YOLO 下载/加载失败时)

正常情况: 真实 YOLO 加载成功, `/health` 返回 `model_loaded=true`, `/detect` 做真实推理。

若 `yolo11n.pt` 下载失败或 `ultralytics` 装不上, 默认行为是:

- `/health` 返回 `model_loaded=false`
- `/detect` 返回 **503** `{"error":"model_not_loaded", ...}`

如果此时仍想验证「板端发图、服务端收图、回 JSON」这条链路 (不做真实检测), 可启用 fake 模式:

```powershell
$env:ENABLE_FAKE_DETECTOR=1
.\scripts\start_server.ps1
```

fake 模式下 `/detect` 会返回 200 + 空 `detections` + 模拟 `inference_ms`, `model_name` 显示 `fake-detector`。**README 已明确说明:fake 模式不做真实目标检测, 仅用于链路联调。**

---

## 8. 接口与错误码速查

| 接口 | 方法 | 说明 |
|------|------|------|
| `/health` | GET | 返回 `{status, model_loaded, model_name}` |
| `/detect` | POST | `multipart/form-data`: `file`(必) + `frame_id`(必) + `capture_ts_ms`(可选) |

`/detect` 成功响应:

```json
{
  "frame_id": 1,
  "capture_ts_ms": 123456789,
  "server_receive_ts_ms": 123456999,
  "inference_ms": 35.2,
  "image_width": 640,
  "image_height": 480,
  "detections": [
    {"class_id":0,"class_name":"person","confidence":0.91,"x1":120,"y1":60,"x2":350,"y2":440}
  ]
}
```

无目标时 `detections` 为空数组 `[]`。错误统一格式 `{"error": <code>, "message": <str>}`:

| 状态码 | error | 触发条件 |
|--------|-------|----------|
| 400 | `invalid_image` | 不是 JPEG (缺 SOI) / 无法解码 / 文件为空 |
| 400 | `invalid_frame_id` | `frame_id` 非整数或为负 |
| 400 | `invalid_capture_ts_ms` | `capture_ts_ms` 提供但非整数或为负 |
| 413 | `file_too_large` | 超过 `MAX_UPLOAD_BYTES` (默认 10MB) |
| 422 | `validation_error` | 缺少 `file` 或 `frame_id` |
| 500 | `inference_error` | 推理抛异常 |
| 503 | `model_not_loaded` | 模型未加载且未开 fake |

---

## 9. 常见错误排查

| 现象 | 原因 / 处理 |
|------|-------------|
| 板端 `curl` 一直卡住 | 防火墙未放行 8080, 见第 6 节 |
| `/health` 通但 `/detect` 超时 | CPU 推理慢, 首帧还会初始化; 等待或换更小输入 |
| `/detect` 返回 503 | 模型未加载; 启用 `ENABLE_FAKE_DETECTOR=1` 先联调链路, 或修复模型下载 |
| `/detect` 返回 400 `invalid_image` | 上传的不是合法 JPEG; 板端不要直接传 YUYV 原始帧 |
| `/detect` 返回 422 | 缺 `file` 或 `frame_id`; 检查 curl 的 `-F` 字段名 |
| `yolo11n.pt` 下载失败 | 网络问题; 手动放到工作目录, 或换 `MODEL_PATH` 指向已有 nano 模型 |
| `ModuleNotFoundError: multipart` | 漏装 `python-multipart`; 重新 `pip install -r requirements.txt` |
| 激活脚本禁止运行 | `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` |

---

## 10. 配置项 (环境变量)

| 变量 | 默认 | 说明 |
|------|------|------|
| `DETECT_HOST` | `0.0.0.0` | 监听地址, 不要改成 `127.0.0.1` |
| `DETECT_PORT` | `8080` | 监听端口 |
| `MODEL_PATH` | `yolo11n.pt` | YOLO 模型 |
| `CONFIDENCE_THRESHOLD` | `0.4` | 置信度阈值 |
| `MAX_UPLOAD_BYTES` | `10485760` | 上传大小上限 (10MB) |
| `ENABLE_FAKE_DETECTOR` | `0` | `1` 启用假检测器联调 |

---

## 11. 项目定位

本服务只是**可替换的推理后端**, 不持续扩展。项目重心仍在板端: 采集与上传 JPEG、网络请求与实时显示隔离、`frame_id` 对应、端到端延迟量化。
