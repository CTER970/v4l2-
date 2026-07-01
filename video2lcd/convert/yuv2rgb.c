
#include <convert_manager.h>
#include <stdlib.h>
#include "color.h"

static int isSupportYuv2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    /* YUYV 是摄像头常见的 YUV422 打包格式, 输出只支持 LCD 常见 RGB565/RGB32 */
    if (iPixelFormatIn != V4L2_PIX_FMT_YUYV)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}

/* YUYV 是 YUV422 打包格式, 每 4 字节 (Y0 U Y1 V) 表示 2 个像素, 两像素共享 U/V */

static unsigned int
Pyuv422torgb565(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned char *output_pt = output_ptr;

    unsigned int r, g, b;
    unsigned int color;

	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V);
		b = B_FROMYU(Y,U);

        /* RGB565: R 取高 5 位, G 取高 6 位, B 取高 5 位 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;

		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V);
		b = B_FROMYU(Y1,U);

        /* 第二个像素复用同一组 U/V, 仅亮度换成 Y1 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
	}

	return 0;
}

/* YUYV -> RGB32, 输出 0x00RRGGBB */

static unsigned int
Pyuv422torgb32(unsigned char * input_ptr, unsigned char * output_ptr, unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned int *output_pt = (unsigned int *)output_ptr;

    unsigned int r, g, b;
    unsigned int color;

	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;

		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V);
		b = B_FROMYU(Y,U);
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;

		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V);
		b = B_FROMYU(Y1,U);
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
	}

	return 0;
}

/* YUV(YUYV) 转 RGB 转换主入口, 参考 luvcview 思路 */
static int Yuv2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
    ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;

    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        ptPixelDatasOut->iBpp = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        /* 输出缓冲区复用, 仅首次 malloc */
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        Pyuv422torgb565(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        ptPixelDatasOut->iBpp = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        Pyuv422torgb32(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }

    return -1;
}

/* 释放转换输出缓冲区内存 */
static int Yuv2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* yuv2rgb 转换模块描述符, 注册到 convert_manager 链表 */
static T_VideoConvert g_tYuv2RgbConvert = {
    .name        = "yuv2rgb",
    .isSupport   = isSupportYuv2Rgb,
    .Convert     = Yuv2RgbConvert,
    .ConvertExit = Yuv2RgbConvertExit,
};

extern void initLut(void);

/* 初始化 YUV 查表并注册 yuv2rgb 转换模块 */
int Yuv2RgbInit(void)
{
    initLut();
    return RegisterVideoConvert(&g_tYuv2RgbConvert);
}
