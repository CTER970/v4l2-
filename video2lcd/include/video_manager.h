
#ifndef _VIDEO_MANAGER_H
#define _VIDEO_MANAGER_H

#include <config.h>
#include <pic_operation.h>
#include <linux/videodev2.h>

#define NB_BUFFER 4

struct VideoDevice;
struct VideoOpr;
typedef struct VideoDevice T_VideoDevice, *PT_VideoDevice;
typedef struct VideoOpr T_VideoOpr, *PT_VideoOpr;

struct VideoDevice {
    /* 摄像头设备文件描述符, 来自 open("/dev/videoX") */
    int iFd;

    /* VIDIOC_S_FMT 后驱动实际采用的格式信息 */
    int iPixelFormat;
    int iWidth;
    int iHeight;

    /* V4L2 buffer 状态:
     * iVideoBufCnt      : 驱动实际分配的 buffer 数量
     * iVideoBufMaxLen   : 单个 buffer 长度, 用于 mmap/munmap
     * iVideoBufCurIndex : 最近一次 DQBUF 取出的 buffer 下标, PutFrame 时用它 QBUF 归还
     * pucVideBuf[]      : mmap 返回的用户空间地址数组, pucVideBuf[index] 才是真正帧数据地址
     */
    int iVideoBufCnt;
    int iVideoBufMaxLen;
    int iVideoBufCurIndex;
    unsigned char *pucVideBuf[NB_BUFFER];

    PT_VideoOpr ptOPr;
};

typedef struct VideoBuf {
    T_PixelDatas tPixelDatas;
    int iPixelFormat;
}T_VideoBuf, *PT_VideoBuf;

struct VideoOpr {
    char *name;
    int (*InitDevice)(char *strDevName, PT_VideoDevice ptVideoDevice);
    int (*ExitDevice)(PT_VideoDevice ptVideoDevice);
    int (*GetFrame)(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*GetFormat)(PT_VideoDevice ptVideoDevice);
    int (*PutFrame)(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
    int (*StartDevice)(PT_VideoDevice ptVideoDevice);
    int (*StopDevice)(PT_VideoDevice ptVideoDevice);
    struct VideoOpr *ptNext;
};


int VideoDeviceInit(char *strDevName, PT_VideoDevice ptVideoDevice);
int V4l2Init(void);
int RegisterVideoOpr(PT_VideoOpr ptVideoOpr);
int VideoInit(void);

int V4l2PrintQuery(const char *dev_name);


#endif /* _VIDEO_MANAGER_H */
