"""cloud_detection_server 本机测试脚本。

- 可直接运行:  python tests/test_client.py
- 也可被 pytest 收集 (函数名以 test_ 开头)

前提: 服务已在本机启动 (.\scripts\start_server.ps1)。
依赖: requests (已在 requirements.txt 中)。
"""
import json
import os
import sys

import requests

BASE = os.environ.get("DETECT_BASE_URL", "http://127.0.0.1:8080")
SAMPLE = os.path.join(os.path.dirname(__file__), "..", "samples", "test.jpg")


def test_health():
    r = requests.get(f"{BASE}/health", timeout=5)
    print("[health]", r.status_code, r.json())
    assert r.status_code == 200
    assert r.json()["status"] == "ok"


def test_detect():
    if not os.path.exists(SAMPLE):
        print("[detect] skip: samples/test.jpg not found")
        return
    with open(SAMPLE, "rb") as f:
        files = {"file": ("test.jpg", f, "image/jpeg")}
        data = {"frame_id": "1", "capture_ts_ms": "123456789"}
        r = requests.post(f"{BASE}/detect", files=files, data=data, timeout=30)
    print("[detect]", r.status_code)
    print(json.dumps(r.json(), ensure_ascii=False, indent=2))
    assert r.status_code == 200
    assert r.json()["frame_id"] == 1


def test_detect_missing_file():
    # 不带 file, 期望 422 统一错误
    r = requests.post(f"{BASE}/detect", data={"frame_id": "1"}, timeout=10)
    print("[missing_file]", r.status_code, r.json())
    assert r.status_code == 422


if __name__ == "__main__":
    ok = True
    for fn in (test_health, test_detect, test_detect_missing_file):
        try:
            fn()
        except Exception as e:
            ok = False
            print(f"[FAIL] {fn.__name__}: {e}")
    print("ALL OK" if ok else "HAS FAILURE")
    sys.exit(0 if ok else 1)
