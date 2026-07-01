#ifndef _DISP_MANAGER_H
#define _DISP_MANAGER_H

#include <pic_operation.h>
#include <video_manager.h>

/* 显示区域,含左上角/右下角座标; 若为图标还含图标文件名 */
typedef struct Layout {
	int iTopLeftX;
	int iTopLeftY;
	int iBotRightX;
	int iBotRightY;
	char *strIconName;
}T_Layout, *PT_Layout;


/* VideoMem 的状态: 空闲/用于预先准备显示内容/用于当前线程 */
typedef enum {
	VMS_FREE = 0,
	VMS_USED_FOR_PREPARE,
	VMS_USED_FOR_CUR,	
}E_VideoMemState;

/* VideoMem 中图片的状态: 空白/正在生成/已经生成 */
typedef enum {
	PS_BLANK = 0,
	PS_GENERATING,
	PS_GENERATED,	
}E_PicState;


typedef struct VideoMem {
	int iID;                        /* ID值,用于标识不同的页面 */
	int bDevFrameBuffer;            /* 1: 是显示设备的显存; 0: 普通缓存 */
	E_VideoMemState eVideoMemState;
	E_PicState ePicState;
	T_PixelDatas tPixelDatas;       /* 内存: 用来存储图像 */
	struct VideoMem *ptNext;        /* 链表 */
}T_VideoMem, *PT_VideoMem;

typedef struct DispOpr {
	char *name;              /* 显示模块的名字 */
	int iXres;               /* 显示设备实际 X 分辨率 */
	int iYres;               /* 显示设备实际 Y 分辨率 */
	int iBpp;                /* framebuffer 每个像素占多少 bit */
	int iLineWidth;          /* framebuffer 一行占多少字节 */
	unsigned char *pucDispMem;   /* mmap 后的显存地址 */
	int (*DeviceInit)(void);     /* 设备初始化函数 */
	int (*ShowPixel)(int iPenX, int iPenY, unsigned int dwColor);    /* 把指定座标的象素设为某颜色 */
	int (*CleanScreen)(unsigned int dwBackColor);                    /* 清屏为某颜色 */
	int (*ShowPage)(PT_PixelDatas ptPixelDatas);                         /* 显示一页,数据源自ptVideoMem */
	struct DispOpr *ptNext;      /* 链表 */
}T_DispOpr, *PT_DispOpr;

/* 注册"显示模块", 把支持的显示设备操作函数放入链表 */
int RegisterDispOpr(PT_DispOpr ptDispOpr);

/* 显示本程序能支持的"显示模块" */
void ShowDispOpr(void);

/* 注册显示设备 */
int DisplayInit(void);

/* 根据名字取出指定的"显示模块", 调用它的初始化函数并清屏 */
void SelectAndInitDefaultDispDev(char *name);

/* 返回由 SelectAndInitDefaultDispDev 选定的默认显示模块 */
PT_DispOpr GetDefaultDispDev(void);

/* 获得所使用显示设备的分辨率和BPP */
int GetDispResolution(int *piXres, int *piYres, int *piBpp);

/* 分配 VideoMem: 为加快显示速度, 先在缓存中构造好页面数据, 显示时再复制到显存 */
int AllocVideoMem(int iNum);

/* 获得显示设备的显存, 在其上操作可直接在 LCD 上显示 */
PT_VideoMem GetDevVideoMem(void);

/* 取一块可操作的 VideoMem 存储待显示数据; bCur=1 必返回, bCur=0 仅用于性能预取 */
PT_VideoMem GetVideoMem(int iID, int bCur);

/* 释放由 GetVideoMem 获取的 VideoMem */
void PutVideoMem(PT_VideoMem ptVideoMem);

/* 把 VideoMem 中全部内存清为某颜色 */
void ClearVideoMem(PT_VideoMem ptVideoMem, unsigned int dwColor);

/* 把 VideoMem 中由 ptLayout 指定的矩形区域清为某颜色 */
void ClearVideoMemRegion(PT_VideoMem ptVideoMem, PT_Layout ptLayout, unsigned int dwColor);

/* 注册"framebuffer 显示设备" */
int FBInit(void);

/* 将默认显示设备的 framebuffer 包装成 T_VideoBuf, 便于和视频帧统一处理 */
int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf);
/* 将一块 PixelDatas 的内容显示到默认显示设备 */
void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas);


/* 查询 framebuffer 参数, 用于 --query 模式的辅助输出 */
int FbPrintInfo(void);
#endif /* _DISP_MANAGER_H */
