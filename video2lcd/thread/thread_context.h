#ifndef _THREAD_CONTEXT_H
#define _THREAD_CONTEXT_H

#include <frame_queue.h>
#include <video_manager.h>
#include <convert_manager.h>

/* P1-1阶段由main创建，在线程全部退出前必须保持有效。 */
typedef struct CaptureContext {
    T_VideoDevice *video_device;
    T_FrameQueue *display_queue;
} T_CaptureContext;

typedef struct DisplayContext {
    T_FrameQueue *display_queue;
    PT_VideoConvert video_convert;
    T_VideoBuf *frame_buf;
} T_DisplayContext;

#endif /* _THREAD_CONTEXT_H */
