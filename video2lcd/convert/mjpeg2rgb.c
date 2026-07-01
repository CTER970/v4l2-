
/* MJPEG : 实质上每一帧数据都是一个完整的JPEG文件 */

#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

typedef struct MyErrorMgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
}T_MyErrorMgr, *PT_MyErrorMgr;

extern void jpeg_mem_src_tj(j_decompress_ptr, unsigned char *, unsigned long);

static int isSupportMjpeg2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    /* MJPEG 输入必须先 JPEG 解码, 输出再转换为 LCD 支持的 RGB565/RGB32 */
    if (iPixelFormatIn != V4L2_PIX_FMT_MJPEG)
        return 0;
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


/* libjpeg 自定义错误处理: 不让默认处理直接 exit, 改为 longjmp 回主流程做清理 */
static void MyErrorExit(j_common_ptr ptCInfo)
{
    static char errStr[JMSG_LENGTH_MAX];
    
	PT_MyErrorMgr ptMyErr = (PT_MyErrorMgr)ptCInfo->err;

    /* 生成 libjpeg 的错误信息, 再 longjmp 回解码主流程做清理 */
    (*ptCInfo->err->format_message) (ptCInfo, errStr);
    DBG_PRINTF("%s\n", errStr);

	longjmp(ptMyErr->setjmp_buffer, 1);
}

/* 把 JPEG 解码得到的单行 RGB888 像素转换为 LCD 需要的 bpp (RGB565/RGB32) */
static int CovertOneLine(int iWidth, int iSrcBpp, int iDstBpp, unsigned char *pudSrcDatas, unsigned char *pudDstDatas)
{
	unsigned int dwRed;
	unsigned int dwGreen;
	unsigned int dwBlue;
	unsigned int dwColor;

	unsigned short *pwDstDatas16bpp = (unsigned short *)pudDstDatas;
	unsigned int   *pwDstDatas32bpp = (unsigned int *)pudDstDatas;

	int i;
	int pos = 0;

	if (iSrcBpp != 24)
	{
		return -1;
	}

	if (iDstBpp == 24)
	{
		memcpy(pudDstDatas, pudSrcDatas, iWidth*3);
	}
	else
	{
		for (i = 0; i < iWidth; i++)
		{
			dwRed   = pudSrcDatas[pos++];
			dwGreen = pudSrcDatas[pos++];
			dwBlue  = pudSrcDatas[pos++];
			if (iDstBpp == 32)
			{
				dwColor = (dwRed << 16) | (dwGreen << 8) | dwBlue;
				*pwDstDatas32bpp = dwColor;
				pwDstDatas32bpp++;
			}
			else if (iDstBpp == 16)
			{
				/* 565 */
				dwRed   = dwRed >> 3;
				dwGreen = dwGreen >> 2;
				dwBlue  = dwBlue >> 3;
				dwColor = (dwRed << 11) | (dwGreen << 5) | (dwBlue);
				*pwDstDatas16bpp = dwColor;
				pwDstDatas16bpp++;
			}
		}
	}
	return 0;
}


//static int GetPixelDatasFrmJPG(PT_FileMap ptFileMap, PT_PixelDatas ptPixelDatas)
/* 把摄像头一帧 MJPEG 数据当作内存中的 JPEG 文件解码为 RGB 图像 */
static int Mjpeg2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	struct jpeg_decompress_struct tDInfo;
	//struct jpeg_error_mgr tJErr;
    int iRet;
    int iRowStride;
    unsigned char *aucLineBuffer = NULL;
    unsigned char *pucDest;
	T_MyErrorMgr tJerr;
    PT_PixelDatas ptPixelDatas = &ptVideoBufOut->tPixelDatas;

	/* libjpeg 默认遇到错误会 exit, 这里用自定义 error_exit + setjmp 避免进程退出 */
	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = MyErrorExit;

	if(setjmp(tJerr.setjmp_buffer))
	{
		/* 如果程序能运行到这里, 表示JPEG解码出错 */
        jpeg_destroy_decompress(&tDInfo);
        if (aucLineBuffer)
        {
            free(aucLineBuffer);
        }
        if (ptPixelDatas->aucPixelDatas)
        {
            free(ptPixelDatas->aucPixelDatas);
            ptPixelDatas->aucPixelDatas = NULL;
        }
		return -1;
	}

	jpeg_create_decompress(&tDInfo);

	/* 数据源不是文件流, 而是 DQBUF 得到的内存帧数据 */
    jpeg_mem_src_tj (&tDInfo, ptVideoBufIn->tPixelDatas.aucPixelDatas, ptVideoBufIn->tPixelDatas.iTotalBytes);
    

    iRet = jpeg_read_header(&tDInfo, TRUE);

	/* 1:1 解码, 不使用 libjpeg 缩放 */
    tDInfo.scale_num = tDInfo.scale_denom = 1;
    
	// 启动解压：jpeg_start_decompress	
	jpeg_start_decompress(&tDInfo);
    
	/* libjpeg 每次输出一行 RGB888 数据, 再转换到目标 LCD bpp */
	iRowStride = tDInfo.output_width * tDInfo.output_components;
	aucLineBuffer = malloc(iRowStride);

    if (NULL == aucLineBuffer)
    {
        jpeg_destroy_decompress(&tDInfo);
        return -1;
    }

	ptPixelDatas->iWidth  = tDInfo.output_width;
	ptPixelDatas->iHeight = tDInfo.output_height;
	//ptPixelDatas->iBpp    = iBpp;
	ptPixelDatas->iLineBytes    = ptPixelDatas->iWidth * ptPixelDatas->iBpp / 8;
    ptPixelDatas->iTotalBytes   = ptPixelDatas->iHeight * ptPixelDatas->iLineBytes;
	if (NULL == ptPixelDatas->aucPixelDatas)
	{
	    ptPixelDatas->aucPixelDatas = malloc(ptPixelDatas->iTotalBytes);
        if (NULL == ptPixelDatas->aucPixelDatas)
        {
            free(aucLineBuffer);
            jpeg_destroy_decompress(&tDInfo);
            return -1;
        }
	}

    pucDest = ptPixelDatas->aucPixelDatas;

	/* 循环读取每一行解码结果并写入输出 buffer */
	while (tDInfo.output_scanline < tDInfo.output_height) 
	{
        /* 得到一行数据,里面的颜色格式为0xRR, 0xGG, 0xBB */
		(void) jpeg_read_scanlines(&tDInfo, &aucLineBuffer, 1);

		/* RGB888 -> RGB565/RGB32 */
		CovertOneLine(ptPixelDatas->iWidth, 24, ptPixelDatas->iBpp, aucLineBuffer, pucDest);
		pucDest += ptPixelDatas->iLineBytes;
	}
	
	free(aucLineBuffer);
	jpeg_finish_decompress(&tDInfo);
	jpeg_destroy_decompress(&tDInfo);

    return 0;
}



static int Mjpeg2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 转换模块描述符: 注册到 convert_manager 的链表中 */
static T_VideoConvert g_tMjpeg2RgbConvert = {
    .name        = "mjpeg2rgb",
    .isSupport   = isSupportMjpeg2Rgb,
    .Convert     = Mjpeg2RgbConvert,
    .ConvertExit = Mjpeg2RgbConvertExit,
};


/* 注册 mjpeg2rgb 转换模块 */
int Mjpeg2RgbInit(void)
{
    return RegisterVideoConvert(&g_tMjpeg2RgbConvert);
}
