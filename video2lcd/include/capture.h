#ifndef _CAPTURE_H
#define _CAPTURE_H

#include <video_manager.h>

/* 保存当前帧到文件, 不做图像编码, 仅写入 ptVideoBuf 的原始字节 */
int SaveFrameAsJpeg(PT_VideoBuf ptVideoBuf, const char *strFilePath);

#endif /* _CAPTURE_H */
