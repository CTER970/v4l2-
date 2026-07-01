
#include <config.h>
#include <disp_manager.h>
#include <string.h>

static PT_DispOpr g_ptDispOprHead;
static PT_DispOpr g_ptDefaultDispOpr;
static PT_VideoMem g_ptVideoMemHead;

/* 注册"显示模块", 把所能支持的显示设备的操作函数放入链表 */
int RegisterDispOpr(PT_DispOpr ptDispOpr)
{
	PT_DispOpr ptTmp;

	/* 显示设备按模块链表注册, 当前主要使用 framebuffer("fb") */
	if (!g_ptDispOprHead)
	{
		g_ptDispOprHead   = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptDispOprHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext	  = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}

	return 0;
}


/* 显示本程序能支持的"显示模块" */
void ShowDispOpr(void)
{
	int i = 0;
	PT_DispOpr ptTmp = g_ptDispOprHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/* 根据名字取出指定的"显示模块" */
PT_DispOpr GetDispOpr(char *pcName)
{
	PT_DispOpr ptTmp = g_ptDispOprHead;
	
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

/* 根据名字取出指定的"显示模块", 调用它的初始化函数, 并且清屏 */
void SelectAndInitDefaultDispDev(char *name)
{
	/* 之后 main.c 通过全局默认显示设备取分辨率/显存 */
	g_ptDefaultDispOpr = GetDispOpr(name);
	if (g_ptDefaultDispOpr)
	{
		g_ptDefaultDispOpr->DeviceInit();
		g_ptDefaultDispOpr->CleanScreen(0);
	}
}



/* 返回事先用 SelectAndInitDefaultDispDev 选定的默认显示模块 */
PT_DispOpr GetDefaultDispDev(void)
{
	return g_ptDefaultDispOpr;
}

/* 获得所使用的显示设备的分辨率和BPP */
int GetDispResolution(int *piXres, int *piYres, int *piBpp)
{
	if (g_ptDefaultDispOpr)
	{
		*piXres = g_ptDefaultDispOpr->iXres;
		*piYres = g_ptDefaultDispOpr->iYres;
		*piBpp  = g_ptDefaultDispOpr->iBpp;
		return 0;
	}
	else
	{
		return -1;
	}
}

int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf)
{
    /* 把 framebuffer 包装成 T_VideoBuf, 让后续 PicMerge/Flush 用统一 PixelDatas 接口处理显示内存 */
    ptFrameBuf->iPixelFormat = (g_ptDefaultDispOpr->iBpp == 16) ? V4L2_PIX_FMT_RGB565 : \
                                   (g_ptDefaultDispOpr->iBpp == 32) ?  V4L2_PIX_FMT_RGB32 : \
                                           0;
    ptFrameBuf->tPixelDatas.iWidth  = g_ptDefaultDispOpr->iXres;
    ptFrameBuf->tPixelDatas.iHeight = g_ptDefaultDispOpr->iYres;
    ptFrameBuf->tPixelDatas.iBpp    = g_ptDefaultDispOpr->iBpp;
    ptFrameBuf->tPixelDatas.iLineBytes    = g_ptDefaultDispOpr->iLineWidth;
    ptFrameBuf->tPixelDatas.iTotalBytes   = ptFrameBuf->tPixelDatas.iLineBytes * ptFrameBuf->tPixelDatas.iHeight;
    ptFrameBuf->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;
    return 0;
}

void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas)
{
    /* ShowPage 对 framebuffer 来说就是把像素数据复制/写入显存 */
    g_ptDefaultDispOpr->ShowPage(ptPixelDatas);
}


/* 为加快显示速度, 事先在缓存(VideoMem)中构造好页面数据, 显示时再复制到设备显存 */
int AllocVideoMem(int iNum)
{
	int i;

	int iXres = 0;
	int iYres = 0;
	int iBpp  = 0;

	int iVMSize;
	int iLineBytes;

	PT_VideoMem ptNew;

	/* 确定VideoMem的大小 */
	GetDispResolution(&iXres, &iYres, &iBpp);
	iVMSize = iXres * iYres * iBpp / 8;
	iLineBytes = iXres * iBpp / 8;

	/* 先把设备本身的framebuffer放入链表: 不另分配像素缓冲, 让 aucPixelDatas 直接指向设备显存 */
	ptNew = malloc(sizeof(T_VideoMem));
	if (ptNew == NULL)
	{
		return -1;
	}

	ptNew->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;

	ptNew->iID = 0;
	ptNew->bDevFrameBuffer = 1;        /* 标记此 VideoMem 即设备本身的 framebuffer, 不参与缓存分配 */
	ptNew->eVideoMemState  = VMS_FREE;
	ptNew->ePicState	   = PS_BLANK;
	ptNew->tPixelDatas.iWidth  = iXres;
	ptNew->tPixelDatas.iHeight = iYres;
	ptNew->tPixelDatas.iBpp    = iBpp;
	ptNew->tPixelDatas.iLineBytes  = iLineBytes;
	ptNew->tPixelDatas.iTotalBytes = iVMSize;

	if (iNum != 0)
	{
		/* 后面要分配缓存 VideoMem, 把设备 framebuffer 的 VideoMem 标记为 VMS_USED_FOR_CUR, 使其不参与缓存分配 */
		ptNew->eVideoMemState = VMS_USED_FOR_CUR;
	}

	ptNew->ptNext = g_ptVideoMemHead;
	g_ptVideoMemHead = ptNew;

	/* 分配用于缓存的 VideoMem */
	for (i = 0; i < iNum; i++)
	{
		/* 一次分配 T_VideoMem 结构体本身 + 跟 framebuffer 同样大小的缓存 */
		ptNew = malloc(sizeof(T_VideoMem) + iVMSize);
		if (ptNew == NULL)
		{
			return -1;
		}
		ptNew->tPixelDatas.aucPixelDatas = (unsigned char *)(ptNew + 1);

		ptNew->iID = 0;
		ptNew->bDevFrameBuffer = 0;
		ptNew->eVideoMemState = VMS_FREE;
		ptNew->ePicState      = PS_BLANK;
		ptNew->tPixelDatas.iWidth  = iXres;
		ptNew->tPixelDatas.iHeight = iYres;
		ptNew->tPixelDatas.iBpp    = iBpp;
		ptNew->tPixelDatas.iLineBytes = iLineBytes;
		ptNew->tPixelDatas.iTotalBytes = iVMSize;

		ptNew->ptNext = g_ptVideoMemHead;
		g_ptVideoMemHead = ptNew;
	}
	
	return 0;
}

/* 获得一块可操作的 VideoMem(存储要显示的数据), 用完后用 PutVideoMem 释放 */
PT_VideoMem GetVideoMem(int iID, int bCur)
{
	PT_VideoMem ptTmp = g_ptVideoMemHead;
	
	/* 1. 优先: 取出空闲的、ID相同的videomem */
	while (ptTmp)
	{
		if ((ptTmp->eVideoMemState == VMS_FREE) && (ptTmp->iID == iID))
		{
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}

	/* 2. 如果前面不成功, 取出一个空闲的并且里面没有数据(ptVideoMem->ePicState = PS_BLANK)的VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if ((ptTmp->eVideoMemState == VMS_FREE) && (ptTmp->ePicState == PS_BLANK))
		{
			ptTmp->iID = iID;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}	
	
	/* 3. 如果前面不成功: 取出任意一个空闲的VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if (ptTmp->eVideoMemState == VMS_FREE)
		{
			ptTmp->iID = iID;
			ptTmp->ePicState = PS_BLANK;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}

    /* 4. 如果没有空闲的VideoMem并且bCur为1, 则取出任意一个VideoMem(不管它是否空闲) */
    if (bCur)
    {
    	ptTmp = g_ptVideoMemHead;
    	ptTmp->iID = iID;
    	ptTmp->ePicState = PS_BLANK;
    	ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
    	return ptTmp;
    }
    
	return NULL;
}

/* 释放 GetVideoMem 取得的 VideoMem */
void PutVideoMem(PT_VideoMem ptVideoMem)
{
	ptVideoMem->eVideoMemState = VMS_FREE;
    if (ptVideoMem->iID == -1)
    {
        ptVideoMem->ePicState = PS_BLANK;  /* iID==-1 表示其中数据已无意义 */
    }
}

/* 获得显示设备本身的显存, 在该显存上操作即可直接显示到 LCD */
PT_VideoMem GetDevVideoMem(void)
{
	PT_VideoMem ptTmp = g_ptVideoMemHead;
	
	while (ptTmp)
	{
		if (ptTmp->bDevFrameBuffer)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}


/* 把 VideoMem 中内存全部清为某种颜色 */
void ClearVideoMem(PT_VideoMem ptVideoMem, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
			memset(pucVM, dwColor, ptVideoMem->tPixelDatas.iTotalBytes);
			break;
		}
		case 16:
		{
			/* 先根据32位的dwColor构造出16位的wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pwVM16bpp	= wColor16bpp;
				pwVM16bpp++;
				i += 2;
			}
			break;
		}
		case 32:
		{
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pdwVM32bpp = dwColor;
				pdwVM32bpp++;
				i += 4;
			}
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}


/* 把 VideoMem 中指定矩形区域全部清为某种颜色 */
void ClearVideoMemRegion(PT_VideoMem ptVideoMem, PT_Layout ptLayout, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int iX;
	int iY;
    int iLineBytesClear;
    int i;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas + ptLayout->iTopLeftY * ptVideoMem->tPixelDatas.iLineBytes + ptLayout->iTopLeftX * ptVideoMem->tPixelDatas.iBpp / 8;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

    iLineBytesClear = (ptLayout->iBotRightX - ptLayout->iTopLeftX + 1) * ptVideoMem->tPixelDatas.iBpp / 8;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
    			memset(pucVM, dwColor, iLineBytesClear);
                pucVM += ptVideoMem->tPixelDatas.iLineBytes;
            }
			break;
		}
		case 16:
		{
			/* 先根据32位的dwColor构造出16位的wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pwVM16bpp[i++]	= wColor16bpp;
    			}
                pwVM16bpp = (unsigned short *)((unsigned int)pwVM16bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		case 32:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pdwVM32bpp[i++]	= dwColor;
    			}
                pdwVM32bpp = (unsigned int *)((unsigned int)pdwVM32bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}

/* 注册显示设备 */
int DisplayInit(void)
{
	int iError;
	
	iError = FBInit();

	return iError;
}
