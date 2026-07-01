#include <config.h>
#include <disp_manager.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>

static int FBDeviceInit(void);
static int FBShowPixel(int iX, int iY, unsigned int dwColor);
static int FBCleanScreen(unsigned int dwBackColor);
static int FBShowPage(PT_PixelDatas ptPixelDatas);


static int g_fd;

static struct fb_var_screeninfo g_tFBVar;
static struct fb_fix_screeninfo g_tFBFix;			
static unsigned char *g_pucFBMem;
static unsigned int g_dwScreenSize;

static unsigned int g_dwLineWidth;
static unsigned int g_dwPixelWidth;

/* framebuffer 显示模块的操作表。
 * disp_manager.c 通过这些函数指针完成显示初始化、清屏和刷屏。
 */
static T_DispOpr g_tFBOpr = {
	.name        = "fb",
	.DeviceInit  = FBDeviceInit,
	.ShowPixel   = FBShowPixel,
	.CleanScreen = FBCleanScreen,
	.ShowPage    = FBShowPage,
};

/* framebuffer 显示设备的初始化: 打开设备、获取屏幕参数、mmap 显存 */
static int FBDeviceInit(void)
{
	int ret;
	
	g_fd = open(FB_DEVICE_NAME, O_RDWR);
	if (0 > g_fd)
	{
		DBG_PRINTF("can't open %s\n", FB_DEVICE_NAME);
	}

	ret = ioctl(g_fd, FBIOGET_VSCREENINFO, &g_tFBVar);
	if (ret < 0)
	{
		DBG_PRINTF("can't get fb's var\n");
		return -1;
	}

	ret = ioctl(g_fd, FBIOGET_FSCREENINFO, &g_tFBFix);
	if (ret < 0)
	{
		DBG_PRINTF("can't get fb's fix\n");
		return -1;
	}
	
	g_dwScreenSize = g_tFBVar.xres * g_tFBVar.yres * g_tFBVar.bits_per_pixel / 8;
	/* mmap 后 g_pucFBMem 指向真实显存, 写这块内存即可改变屏幕显示 */
	g_pucFBMem = (unsigned char *)mmap(NULL , g_dwScreenSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
	if (0 > g_pucFBMem)	
	{
		DBG_PRINTF("can't mmap\n");
		return -1;
	}

	g_tFBOpr.iXres       = g_tFBVar.xres;
	g_tFBOpr.iYres       = g_tFBVar.yres;
	g_tFBOpr.iBpp        = g_tFBVar.bits_per_pixel;
	g_tFBOpr.iLineWidth  = g_tFBVar.xres * g_tFBOpr.iBpp / 8;
	g_tFBOpr.pucDispMem  = g_pucFBMem;

	g_dwLineWidth  = g_tFBVar.xres * g_tFBVar.bits_per_pixel / 8;
	g_dwPixelWidth = g_tFBVar.bits_per_pixel / 8;
	
	return 0;
}


/* 设置 FrameBuffer 指定像素的颜色, dwColor 为 0x00RRGGBB */
static int FBShowPixel(int iX, int iY, unsigned int dwColor)
{
	unsigned char *pucFB;
	unsigned short *pwFB16bpp;
	unsigned int *pdwFB32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;

	if ((iX >= g_tFBVar.xres) || (iY >= g_tFBVar.yres))
	{
		DBG_PRINTF("out of region\n");
		return -1;
	}

	/* 根据二维坐标计算 framebuffer 中的一维字节地址 */
	pucFB      = g_pucFBMem + g_dwLineWidth * iY + g_dwPixelWidth * iX;
	pwFB16bpp  = (unsigned short *)pucFB;
	pdwFB32bpp = (unsigned int *)pucFB;
	
	switch (g_tFBVar.bits_per_pixel)
	{
		case 8:
		{
			*pucFB = (unsigned char)dwColor;
			break;
		}
		case 16:
		{
			/* 从dwBackColor中取出红绿蓝三原色,
			 * 构造为16Bpp的颜色
			 */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			*pwFB16bpp	= wColor16bpp;
			break;
		}
		case 32:
		{
			*pdwFB32bpp = dwColor;
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}
	}

	return 0;
}

/* 将整屏像素数据 memcpy 到 framebuffer 显存 */
static int FBShowPage(PT_PixelDatas ptPixelDatas)
{
    /* 如果源数据已经指向 framebuffer, 就不需要重复 memcpy */
    if (g_tFBOpr.pucDispMem != ptPixelDatas->aucPixelDatas)
    {
    	memcpy(g_tFBOpr.pucDispMem, ptPixelDatas->aucPixelDatas, ptPixelDatas->iTotalBytes);
    }
	return 0;
}

/* 用 dwBackColor 清屏 */
static int FBCleanScreen(unsigned int dwBackColor)
{
	unsigned char *pucFB;
	unsigned short *pwFB16bpp;
	unsigned int *pdwFB32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	pucFB      = g_pucFBMem;
	pwFB16bpp  = (unsigned short *)pucFB;
	pdwFB32bpp = (unsigned int *)pucFB;

	switch (g_tFBVar.bits_per_pixel)
	{
		case 8:
		{
			memset(g_pucFBMem, dwBackColor, g_dwScreenSize);
			break;
		}
		case 16:
		{
			/* 从dwBackColor中取出红绿蓝三原色,
			 * 构造为16Bpp的颜色
			 */
			iRed   = (dwBackColor >> (16+3)) & 0x1f;
			iGreen = (dwBackColor >> (8+2)) & 0x3f;
			iBlue  = (dwBackColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < g_dwScreenSize)
			{
				*pwFB16bpp	= wColor16bpp;
				pwFB16bpp++;
				i += 2;
			}
			break;
		}
		case 32:
		{
			while (i < g_dwScreenSize)
			{
				*pdwFB32bpp	= dwBackColor;
				pdwFB32bpp++;
				i += 4;
			}
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}
	}

	return 0;
}

/* 注册 framebuffer 显示设备到 disp_manager */
int FBInit(void)
{
	return RegisterDispOpr(&g_tFBOpr);
}



int FbPrintInfo(void)
{
int ret;
int fd_dis;
static struct fb_var_screeninfo g_tFBVar;

	/* 查询模式下单独打开 framebuffer, 只打印显示参数, 不初始化完整显示链路 */
	fd_dis = open(FB_DEVICE_NAME, O_RDWR);
	if (0 > fd_dis)
	{
		DBG_PRINTF("can't open %s\n", FB_DEVICE_NAME);
		return -1;
	}
	

	ret = ioctl(fd_dis, FBIOGET_VSCREENINFO, &g_tFBVar);
	if (ret < 0)
	{
		DBG_PRINTF("can't get fb's var\n");
		close(fd_dis);
		return -1;
	}

	{
		DBG_PRINTF("__u32 xres is %u\n",g_tFBVar. xres);
		DBG_PRINTF("__u32 yres is %u\n",g_tFBVar. yres);
		DBG_PRINTF("__u32 bits_per_pixel is %u\n",g_tFBVar. bits_per_pixel);
		close(fd_dis);
		return 0;
	}

}
