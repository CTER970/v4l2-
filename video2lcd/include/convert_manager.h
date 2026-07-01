
#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <config.h>
#include <video_manager.h>
#include <linux/videodev2.h>

typedef struct VideoConvert {
    char *name;
    /* 判断该模块是否支持 input -> output 这组像素格式 */
    int (*isSupport)(int iPixelFormatIn, int iPixelFormatOut);
    /* 执行真实转换, 输入来自摄像头帧, 输出通常是 framebuffer 可显示格式 */
    int (*Convert)(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);
    /* 释放 Convert 中为输出 buffer 分配的内存 */
    int (*ConvertExit)(PT_VideoBuf ptVideoBufOut);
    struct VideoConvert *ptNext;
}T_VideoConvert, *PT_VideoConvert;

PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut);
int VideoConvertInit(void);

int Yuv2RgbInit(void);
int Mjpeg2RgbInit(void);
int Rgb2RgbInit(void);
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert);


#endif /* _CONVERT_MANAGER_H */
