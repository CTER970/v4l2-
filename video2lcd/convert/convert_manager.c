
#include <config.h>
#include <convert_manager.h>
#include <string.h>

static PT_VideoConvert g_ptVideoConvertHead = NULL;

/* 注册一个像素格式转换模块, 例如 yuv2rgb/mjpeg2rgb/rgb2rgb */
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert)
{
	PT_VideoConvert ptTmp;

	/* 转换模块也使用链表管理。
	 * main.c 只按输入/输出格式查找合适模块, 不关心具体转换实现。
	 */
	if (!g_ptVideoConvertHead)
	{
		g_ptVideoConvertHead   = ptVideoConvert;
		ptVideoConvert->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptVideoConvertHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext     = ptVideoConvert;
		ptVideoConvert->ptNext = NULL;
	}

	return 0;
}


/* 显示本程序已经注册的格式转换模块 */
void ShowVideoConvert(void)
{
	int i = 0;
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/* 根据名字取出指定的格式转换模块 */
PT_VideoConvert GetVideoConvert(char *pcName)
{
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;
	
	while (ptTmp)
	{
		if (strcmp(ptTmp->name, pcName) == 0)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}

PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut)
{
	PT_VideoConvert ptTmp = g_ptVideoConvertHead;
	
    /* 使用驱动实际输入格式和 LCD 输出格式，逐个查询已注册转换模块。 */
	while (ptTmp)
	{
        if (ptTmp->isSupport(iPixelFormatIn, iPixelFormatOut))
        {
            return ptTmp;
        }
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}


/* 初始化转换子系统, 注册项目支持的格式转换模块 */
int VideoConvertInit(void)
{
	int iError;

    /* 注册顺序会影响同一格式存在多个实现时的优先级 */
    iError = Yuv2RgbInit();
    iError |= Mjpeg2RgbInit();
    iError |= Rgb2RgbInit();

	return iError;
}



