# Windows 目标检测服务端

本项目是 `video2lcd` i.MX6ULL 项目的可替换推理后端。服务端每次接收一张 JPEG 图片，使用 YOLO 完成目标检测，并以 JSON 返回检测结果。

## 1. 接口说明

- `GET /health`：查询服务与模型加载状态。
- `POST /detect`：上传 `multipart/form-data` 表单，包含 `file`、`frame_id` 和可选的 `capture_ts_ms`。
- 在线接口文档：`http://127.0.0.1:8080/docs`。

服务启动时只加载一次模型，后续请求复用同一个模型实例。默认使用 CPU 推理，并监听 `0.0.0.0:8080`，允许局域网内的开发板访问。

## 2. Windows 安装

在本目录中执行：

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

如果 PowerShell 禁止激活脚本，可以直接使用 `.\.venv\Scripts\python.exe`，或者在确认影响后调整当前用户的执行策略：

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

不要修改系统级执行策略。

## 3. 配置参数

| 环境变量 | 默认值 | 说明 |
| --- | --- | --- |
| `DETECT_HOST` | `0.0.0.0` | 服务监听地址 |
| `DETECT_PORT` | `8080` | 服务监听端口 |
| `MODEL_PATH` | `yolo11n.pt` | YOLO 模型路径 |
| `CONFIDENCE_THRESHOLD` | `0.40` | 最低检测置信度 |
| `MAX_UPLOAD_BYTES` | `10485760` | 单张图片最大字节数 |

如果本地不存在 `yolo11n.pt`，Ultralytics 会在首次启动时下载模型。也可以通过 `MODEL_PATH` 指定已有模型文件。

## 4. 启动服务

```powershell
.\scripts\start_server.ps1
```

健康检查：

```powershell
curl.exe http://127.0.0.1:8080/health
```

上传 JPEG：

```powershell
curl.exe -X POST -F "file=@samples/test.jpg" -F "frame_id=1" http://127.0.0.1:8080/detect
python .\scripts\test_client.py .\samples\test.jpg --frame-id 1
```

## 5. 自动化测试

测试使用假检测器替换真实 YOLO，因此不会重复加载或下载模型：

```powershell
python -m pytest -q
```

## 6. i.MX6ULL 局域网联调

先在 Windows 执行 `ipconfig`，找到与开发板位于同一网段的 IPv4 地址。假设 Windows 地址为 `192.168.1.100`，在开发板执行：

```bash
ping 192.168.1.100
curl http://192.168.1.100:8080/health
curl -X POST \
  -F "file=@capture.jpg" \
  -F "frame_id=1" \
  -F "capture_ts_ms=1710000000000" \
  http://192.168.1.100:8080/detect
```

`capture.jpg` 必须是真实 JPEG 或摄像头输出的 MJPEG 帧。将 YUYV 原始数据直接改名为 `.jpg` 无法被服务端解码。

当前板端 `UploadFileToServer` 已按服务端协议提交 `file`、`frame_id`，并在采集时间戳有效时提交 `capture_ts_ms`。联调前只需要确认板端代码中的服务端 URL 使用实际 Windows IPv4 地址，并指向 `/detect`。

## 7. Windows 防火墙

仅当局域网设备无法连接时，在管理员 PowerShell 中检查并按需执行：

```powershell
New-NetFirewallRule `
  -DisplayName "video2lcd detection server" `
  -Direction Inbound `
  -Protocol TCP `
  -LocalPort 8080 `
  -Action Allow
```

项目不会自动修改 Windows 防火墙。

## 8. 常见问题

- `/health` 返回 `model_loaded=false`：查看启动日志，确认首次模型下载网络正常，或通过 `MODEL_PATH` 指定本地 `.pt` 文件。
- 开发板无法连接：确认服务监听 `0.0.0.0`、双方网络互通，并检查 Windows 防火墙是否允许 TCP 8080 端口。
- 返回 `invalid_image`：确认文件内容是真实 JPEG。服务端不会只根据 Content-Type 判断图片是否合法。
- 第一次请求较慢：CPU 首次推理和模型初始化通常比后续请求耗时更长。
