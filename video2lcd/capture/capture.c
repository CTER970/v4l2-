#include <stdio.h>
#include "capture.h"

/* 按原始帧数据直接落盘: MJPG 时即为标准 JPEG; YUYV/RGB565 时不做编码, 仅文件名后缀为 .jpg */
int SaveFrameAsJpeg(PT_VideoBuf ptVideoBuf, const char *strFilePath)
{
    FILE *fp;
    size_t expected_bytes;
    size_t written_bytes;

    if (!ptVideoBuf || !ptVideoBuf->tPixelDatas.aucPixelDatas || !strFilePath ||
        ptVideoBuf->tPixelDatas.iTotalBytes <= 0)
    {
        printf("SaveFrameAsJpeg: invalid parameter\n");
        return -1;
    }

    fp = fopen(strFilePath, "wb");
    if (!fp)
    {
        printf("SaveFrameAsJpeg: can not open %s\n", strFilePath);
        return -1;
    }

    expected_bytes = (size_t)ptVideoBuf->tPixelDatas.iTotalBytes;
    written_bytes = fwrite(ptVideoBuf->tPixelDatas.aucPixelDatas, 1,
                           expected_bytes, fp);
    if (written_bytes != expected_bytes)
    {
        printf("SaveFrameAsJpeg: short write, expected %lu bytes, wrote %lu bytes\n",
               (unsigned long)expected_bytes, (unsigned long)written_bytes);
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0)
    {
        printf("SaveFrameAsJpeg: close %s failed\n", strFilePath);
        return -1;
    }

    printf("SaveFrameAsJpeg: saved %d bytes to %s\n",
           ptVideoBuf->tPixelDatas.iTotalBytes, strFilePath);
    return 0;
}
