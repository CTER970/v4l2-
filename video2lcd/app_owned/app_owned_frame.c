#include <stdlib.h>
#include <string.h>

#include "app_owned_frame.h"

T_AppOwnedFrame *AppOwnedFrameCreateFromVideoBuf(
    const T_VideoBuf *src,
    unsigned long frame_id,
    long long ts_ms)
{
    T_AppOwnedFrame *frame;
    unsigned char *data;
    int size;

    /* src 必须描述一帧有效图像，数据地址不能为空。 */
    if (!src || !src->tPixelDatas.aucPixelDatas)
        return NULL;

    /* iTotalBytes 来自 V4L2 DQBUF 返回的 bytesused。 */
    size = src->tPixelDatas.iTotalBytes;
    if (size <= 0)
        return NULL;

    /* 第一块内存用于保存帧描述、帧编号和采集时间戳。 */
    frame = malloc(sizeof(*frame));
    if (!frame)
        return NULL;

    /* 第二块内存用于保存真正的图像数据，大小只取当前帧有效字节数。 */
    data = malloc((size_t)size);
    if (!data)
    {
        /* 图像内存申请失败时，释放已经申请的外层结构体。 */
        free(frame);
        return NULL;
    }

    /* 必须在采集线程执行 QBUF 之前完成深拷贝。 */
    memcpy(data, src->tPixelDatas.aucPixelDatas, (size_t)size);

    /*
     * 结构体赋值会复制宽、高、格式、大小等所有图像描述字段，
     * 同时也会浅拷贝 src 中的 mmap 地址。所以下一行必须立即把
     * 数据指针替换为刚才 malloc 并完成深拷贝的应用内存地址。
     */
    frame->owned_video_buf = *src;
    frame->owned_video_buf.tPixelDatas.aucPixelDatas = data;

    /* 保存随帧传递的业务信息，不需要转换成字符串。 */
    frame->frame_id = frame_id;
    frame->ts_ms = ts_ms;

    return frame;
}

void AppOwnedFrameDestroy(T_AppOwnedFrame *frame)
{
    if (!frame)
        return;

    /* owned_video_buf 的数据指针由本模块申请，因此由本模块释放。 */
    free(frame->owned_video_buf.tPixelDatas.aucPixelDatas);

    /* 图像数据释放后，再释放包含描述信息的外层帧结构体。 */
    free(frame);
}
