#include <stdlib.h>
#include <string.h>

#include "display_thread.h"
#include "thread_context.h"

#include <app_owned_frame.h>
#include <config.h>
#include <disp_manager.h>
#include <frame_queue.h>
#include <render.h>

static int PrepareZoomBuf(PT_VideoBuf zoom_buf,
                          PT_VideoBuf src_buf,
                          PT_VideoBuf frame_buf)
{
    float ratio;
    int lcd_width;
    int lcd_height;
    int old_total_bytes;

    if (!zoom_buf || !src_buf || !frame_buf)
        return -1;

    lcd_width = frame_buf->tPixelDatas.iWidth;
    lcd_height = frame_buf->tPixelDatas.iHeight;

    if (src_buf->tPixelDatas.iWidth <= 0 ||
        src_buf->tPixelDatas.iHeight <= 0 ||
        src_buf->tPixelDatas.iBpp <= 0)
    {
        return -1;
    }

    ratio = (float)src_buf->tPixelDatas.iHeight /
            (float)src_buf->tPixelDatas.iWidth;

    old_total_bytes = zoom_buf->tPixelDatas.iTotalBytes;

    zoom_buf->iPixelFormat = frame_buf->iPixelFormat;
    zoom_buf->tPixelDatas.iWidth = lcd_width;
    zoom_buf->tPixelDatas.iHeight = (int)(lcd_width * ratio);
    if (zoom_buf->tPixelDatas.iHeight > lcd_height)
    {
        zoom_buf->tPixelDatas.iWidth = (int)(lcd_height / ratio);
        zoom_buf->tPixelDatas.iHeight = lcd_height;
    }

    zoom_buf->tPixelDatas.iBpp = frame_buf->tPixelDatas.iBpp;
    zoom_buf->tPixelDatas.iLineBytes =
        zoom_buf->tPixelDatas.iWidth * zoom_buf->tPixelDatas.iBpp / 8;
    zoom_buf->tPixelDatas.iTotalBytes =
        zoom_buf->tPixelDatas.iLineBytes * zoom_buf->tPixelDatas.iHeight;

    if (zoom_buf->tPixelDatas.iTotalBytes <= 0)
        return -1;

    if (zoom_buf->tPixelDatas.aucPixelDatas &&
        old_total_bytes < zoom_buf->tPixelDatas.iTotalBytes)
    {
        free(zoom_buf->tPixelDatas.aucPixelDatas);
        zoom_buf->tPixelDatas.aucPixelDatas = NULL;
    }

    if (!zoom_buf->tPixelDatas.aucPixelDatas)
    {
        zoom_buf->tPixelDatas.aucPixelDatas =
            malloc((size_t)zoom_buf->tPixelDatas.iTotalBytes);
        if (!zoom_buf->tPixelDatas.aucPixelDatas)
            return -1;
    }

    return 0;
}

void *DisplayThread(void *arg)
{
    T_DisplayContext *ctx = arg;
    T_AppOwnedFrame *frame;
    PT_VideoBuf cur_buf;
    T_VideoBuf convert_buf;
    T_VideoBuf zoom_buf;
    int ret;
    int top_left_x;
    int top_left_y;

    if (!ctx || !ctx->display_queue || !ctx->frame_buf)
        return NULL;

    memset(&convert_buf, 0, sizeof(convert_buf));
    memset(&zoom_buf, 0, sizeof(zoom_buf));

    convert_buf.iPixelFormat = ctx->frame_buf->iPixelFormat;
    convert_buf.tPixelDatas.iBpp = ctx->frame_buf->tPixelDatas.iBpp;

    while (1)
    {
        frame = NULL;
        ret = FrameQueuePop(ctx->display_queue, &frame);
        if (ret == FRAME_QUEUE_STOPPED)
            break;
        if (ret != FRAME_QUEUE_OK)
        {
            DBG_PRINTF("DisplayThread: FrameQueuePop failed, ret=%d\n", ret);
            break;
        }
        if (!frame)
            continue;

        cur_buf = &frame->owned_video_buf;

        if (cur_buf->iPixelFormat != ctx->frame_buf->iPixelFormat)
        {
            if (!ctx->video_convert)
            {
                DBG_PRINTF("DisplayThread: missing video converter, frame_id=%lu\n",
                           frame->frame_id);
                AppOwnedFrameDestroy(frame);
                continue;
            }

            ret = ctx->video_convert->Convert(cur_buf, &convert_buf);
            if (ret)
            {
                DBG_PRINTF("DisplayThread: Convert %s failed, ret=%d, frame_id=%lu\n",
                           ctx->video_convert->name, ret, frame->frame_id);
                AppOwnedFrameDestroy(frame);
                continue;
            }
            cur_buf = &convert_buf;
        }

        if (cur_buf->tPixelDatas.iWidth > ctx->frame_buf->tPixelDatas.iWidth ||
            cur_buf->tPixelDatas.iHeight > ctx->frame_buf->tPixelDatas.iHeight)
        {
            ret = PrepareZoomBuf(&zoom_buf, cur_buf, ctx->frame_buf);
            if (ret)
            {
                DBG_PRINTF("DisplayThread: PrepareZoomBuf failed, frame_id=%lu\n",
                           frame->frame_id);
                AppOwnedFrameDestroy(frame);
                continue;
            }

            ret = PicZoom(&cur_buf->tPixelDatas, &zoom_buf.tPixelDatas);
            if (ret)
            {
                DBG_PRINTF("DisplayThread: PicZoom failed, ret=%d, frame_id=%lu\n",
                           ret, frame->frame_id);
                AppOwnedFrameDestroy(frame);
                continue;
            }
            cur_buf = &zoom_buf;
        }

        top_left_x =
            (ctx->frame_buf->tPixelDatas.iWidth - cur_buf->tPixelDatas.iWidth) / 2;
        top_left_y =
            (ctx->frame_buf->tPixelDatas.iHeight - cur_buf->tPixelDatas.iHeight) / 2;

        ret = PicMerge(top_left_x, top_left_y,
                       &cur_buf->tPixelDatas,
                       &ctx->frame_buf->tPixelDatas);
        if (ret)
        {
            DBG_PRINTF("DisplayThread: PicMerge failed, ret=%d, frame_id=%lu\n",
                       ret, frame->frame_id);
            AppOwnedFrameDestroy(frame);
            continue;
        }

        FlushPixelDatasToDev(&ctx->frame_buf->tPixelDatas);
        AppOwnedFrameDestroy(frame);
    }

    if (ctx->video_convert && ctx->video_convert->ConvertExit)
        ctx->video_convert->ConvertExit(&convert_buf);

    free(zoom_buf.tPixelDatas.aucPixelDatas);
    zoom_buf.tPixelDatas.aucPixelDatas = NULL;

    return NULL;
}
