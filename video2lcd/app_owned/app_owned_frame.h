#ifndef _APP_OWNED_FRAME_H
#define _APP_OWNED_FRAME_H

#include <video_manager.h>

typedef struct AppOwnedFrame {
    /* 复用项目原有 T_VideoBuf，但 aucPixelDatas 指向应用层 malloc 的独立内存，不再指向 V4L2 mmap buffer */
    T_VideoBuf owned_video_buf;

    /* 帧编号和采集时间戳用于日志、丢帧统计、延迟计算及上传校验 */
    unsigned long frame_id;
    long long ts_ms;
} T_AppOwnedFrame;

/* 深拷贝 src 创建一帧应用自有副本，返回后调用者即可 QBUF 归还驱动 buffer 而不影响新帧 */
T_AppOwnedFrame *AppOwnedFrameCreateFromVideoBuf(
    const T_VideoBuf *src,
    unsigned long frame_id,
    long long ts_ms);

/* 释放一帧应用自有数据（允许传 NULL，每帧只能销毁一次） */
void AppOwnedFrameDestroy(T_AppOwnedFrame *frame);

#endif /* _APP_OWNED_FRAME_H */
