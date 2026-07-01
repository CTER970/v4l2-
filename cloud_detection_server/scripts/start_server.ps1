$ErrorActionPreference = "Stop"

# 无论从哪个目录调用脚本，都先切换到服务端项目根目录。
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

# 仅在用户未显式配置时设置默认值。
if (-not $env:DETECT_HOST) { $env:DETECT_HOST = "0.0.0.0" }
if (-not $env:DETECT_PORT) { $env:DETECT_PORT = "8080" }
if (-not $env:MODEL_PATH) { $env:MODEL_PATH = "yolo11n.pt" }
if (-not $env:CONFIDENCE_THRESHOLD) { $env:CONFIDENCE_THRESHOLD = "0.40" }
if (-not $env:MAX_UPLOAD_BYTES) { $env:MAX_UPLOAD_BYTES = "10485760" }

python -m uvicorn app.main:app --host $env:DETECT_HOST --port $env:DETECT_PORT
