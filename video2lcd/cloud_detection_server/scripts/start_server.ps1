# 启动 cloud_detection_server (Windows 目标检测服务)
# 用法:  .\scripts\start_server.ps1
# 等价:  uvicorn app.main:app --host 0.0.0.0 --port 8080

$ErrorActionPreference = "Stop"

# 切换到项目根目录 (脚本位于 scripts/, 父目录即项目根)
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -Path $ProjectRoot

$VenvPython = ".\.venv\Scripts\python.exe"
if (Test-Path $VenvPython) {
    Write-Host "[start_server] using venv: $VenvPython" -ForegroundColor Green
    & $VenvPython -m uvicorn app.main:app --host 0.0.0.0 --port 8080
} else {
    Write-Host "[start_server] .venv not found, using system 'python'." -ForegroundColor Yellow
    Write-Host "[start_server] 建议先创建虚拟环境:  py -3.11 -m venv .venv" -ForegroundColor Yellow
    python -m uvicorn app.main:app --host 0.0.0.0 --port 8080
}
