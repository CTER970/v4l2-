"""
假云端服务 - 嵌入式视觉核验前端终端 MVP
功能：接收开发板上传的图片，返回固定 JSON 结果
运行：python3 server.py
访问：http://<Ubuntu_IP>:8080/detect
"""

from flask import Flask, request, jsonify
import os
from datetime import datetime

app = Flask(__name__)

# 上传文件保存目录（相对路径，运行时自动创建）
UPLOAD_DIR = "uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)


@app.route("/upload", methods=["POST"])
def upload_file():
    # 校验1：请求中必须包含名为 "file" 的文件字段
    if "file" not in request.files:
        return jsonify({
            "success": False,
            "status": "no_file",
            "msg": "no file uploaded"
        }), 400

    file = request.files["file"]

    # 校验2：文件名不能为空
    if file.filename == "":
        return jsonify({
            "success": False,
            "status": "empty_filename",
            "msg": "empty filename"
        }), 400

    # 用时间戳命名，避免多次上传覆盖同一文件
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    save_name = f"{timestamp}_{file.filename}"
    save_path = os.path.join(UPLOAD_DIR, save_name)

    # 保存上传的图片到本地
    file.save(save_path)
    print(f"[INFO] file saved: {save_path}")

    # 返回固定 JSON（假云端，不接真实识别）
    return jsonify({
        "success": True,
        "status": "pass",
        "name": "test_user"
    })


if __name__ == "__main__":
    # host="0.0.0.0" 监听所有网卡，允许开发板通过局域网访问
    app.run(host="0.0.0.0", port=8080, debug=True)
