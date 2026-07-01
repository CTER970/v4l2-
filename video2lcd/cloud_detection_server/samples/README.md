# samples 测试图片目录

本目录存放本机测试与板端联调用的 JPEG 图片。

## 放置测试图片

把一张真实 JPEG 重命名为 `test.jpg` 放到本目录, 供 `tests/test_client.py` 上传:

```
cloud_detection_server\samples\test.jpg
```

## 没有现成图片? 用 OpenCV 生成一张合成测试图

在项目根目录 (已激活 `.venv`) 执行下面这条命令, 生成一张 640x480 灰底红字 `TEST` 的合法 JPEG:

```powershell
python -c "import cv2, numpy as np; img = np.zeros((480,640,3), np.uint8); img[:] = (200,200,200); cv2.putText(img,'TEST',(120,260),cv2.FONT_HERSHEY_SIMPLEX,3,(0,0,255),5); cv2.imwrite('samples/test.jpg', img)"
```

合成图检测不到真实目标时, `detections` 返回空数组, 属正常现象, 仍可验证解码与端云收发链路。

## 板端图片

板端应上传摄像头 MJPEG 帧保存得到的真实 JPEG (`capture.jpg`), 不要直接上传 YUYV 原始帧 —— YUYV 不是合法 JPEG, 服务端会返回 400 `invalid_image`。
