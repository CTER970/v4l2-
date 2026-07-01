#include <sys/time.h>
#include <string.h>

#include "capture_thread.h"
#include "thread_context.h"
#include <app_owned_frame.h>
#include <config.h>
#include <frame_queue.h>

static long long GetTimeMs(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return -1;

    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void *CaptureThread(void *arg)
{
    T_CaptureContext *capture_ctx = arg;
    T_VideoBuf video_buf;
    T_AppOwnedFrame *frame;
    unsigned long frame_id = 0;
    long long ts_ms;
    int ret;

    if (!capture_ctx || !capture_ctx->video_device || !capture_ctx->display_queue)
        return NULL;

    memset(&video_buf, 0, sizeof(video_buf));

    while (1)
    {
        ret = capture_ctx->video_device->ptOPr->GetFrame(
            capture_ctx->video_device, &video_buf);
        if (ret)
        {
            DBG_PRINTF("CaptureThread: GetFrame failed\n");
            FrameQueueStop(capture_ctx->display_queue);
            return NULL;
        }

        frame_id++;
        ts_ms = GetTimeMs();

        frame = AppOwnedFrameCreateFromVideoBuf(&video_buf, frame_id, ts_ms);

        ret = capture_ctx->video_device->ptOPr->PutFrame(
            capture_ctx->video_device, &video_buf);
        if (ret)
        {
            DBG_PRINTF("CaptureThread: PutFrame failed\n");
            AppOwnedFrameDestroy(frame);
            FrameQueueStop(capture_ctx->display_queue);
            return NULL;
        }

        if (!frame)
        {
            DBG_PRINTF("CaptureThread: drop frame %lu, deep copy failed\n", frame_id);
            continue;
        }

        ret = FrameQueuePush(capture_ctx->display_queue, frame);
        if (ret != FRAME_QUEUE_OK)
        {
            AppOwnedFrameDestroy(frame);

            if (ret == FRAME_QUEUE_FULL)
                continue;

            break;
        }
    }

    FrameQueueStop(capture_ctx->display_queue);
    return NULL;
}
