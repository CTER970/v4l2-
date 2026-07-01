import argparse
import json
from pathlib import Path

import requests


def main() -> int:
    """上传一张 JPEG 并打印服务端返回的格式化 JSON。"""
    parser = argparse.ArgumentParser(description="向目标检测服务上传一张 JPEG 图片")
    parser.add_argument("image", type=Path, help="JPEG 图片路径")
    parser.add_argument("--url", default="http://127.0.0.1:8080/detect")
    parser.add_argument("--frame-id", type=int, default=1)
    parser.add_argument("--capture-ts-ms", type=int)
    args = parser.parse_args()

    # multipart 表单中的普通字段放入 data，图片文件放入 files。
    data = {"frame_id": args.frame_id}
    if args.capture_ts_ms is not None:
        data["capture_ts_ms"] = args.capture_ts_ms

    with args.image.open("rb") as image_file:
        response = requests.post(
            args.url,
            files={"file": (args.image.name, image_file, "image/jpeg")},
            data=data,
            timeout=60,
        )

    print(f"HTTP {response.status_code}")
    try:
        print(json.dumps(response.json(), ensure_ascii=False, indent=2))
    except requests.JSONDecodeError:
        print(response.text)
    return 0 if response.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
