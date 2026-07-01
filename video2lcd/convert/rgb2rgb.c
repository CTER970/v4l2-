
#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>

static int isSupportRgb2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    /* 摄像头实际输出已经是 RGB565 时, 只需要复制或扩展到 LCD 需要的 RGB 格式 */
    if (iPixelFormatIn != V4L2_PIX_FMT_RGB565)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}

static int Rgb2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{   
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    int x, y;
    int r, g, b;
    int color;
    unsigned short *pwSrc = (unsigned short *)ptPixelDatasIn->aucPixelDatas;
    unsigned int *pdwDest;

    if (ptVideoBufIn->iPixelFormat != V4L2_PIX_FMT_RGB565)
    {
        return -1;
    }

    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        /* 输入输出都是 RGB565: 只需要准备输出 buffer 并复制整帧数据 */
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        memcpy(ptPixelDatasOut->aucPixelDatas, ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->iTotalBytes);
        return 0;
    }
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        /* RGB565 -> RGB32: 每个 16 位像素拆出 R/G/B 后扩展为 0x00RRGGBB */
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        pdwDest = (unsigned int *)ptPixelDatasOut->aucPixelDatas;
        
        for (y = 0; y < ptPixelDatasOut->iHeight; y++)
        {
            for (x = 0; x < ptPixelDatasOut->iWidth; x++)
            {
                color = *pwSrc++;
                r = color >> 11;
                g = (color >> 5) & (0x3f);
                b = color & 0x1f;

                color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);

                *pdwDest = color;
                pdwDest++;
            }
        }
        return 0;
    }

    return -1;
}

static int Rgb2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

static T_VideoConvert g_tRgb2RgbConvert = {
    .name        = "rgb2rgb",
    .isSupport   = isSupportRgb2Rgb,
    .Convert     = Rgb2RgbConvert,
    .ConvertExit = Rgb2RgbConvertExit,
};


int Rgb2RgbInit(void)
{
    return RegisterVideoConvert(&g_tRgb2RgbConvert);
}


