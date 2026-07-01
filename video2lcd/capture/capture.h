#ifndef _CAPTURE_H
#define _CAPTURE_H

#include <video_manager.h>

/* 把 ptVideoBuf 中的原始字节写入文件, 不做图像编码 */
int SaveFrameAsJpeg(PT_VideoBuf ptVideoBuf, const char *strFilePath);

#endif /* _CAPTURE_H */
