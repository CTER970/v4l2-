
#include <config.h>
#include <video_manager.h>
#include <string.h>

static PT_VideoOpr g_ptVideoOprHead = NULL;

/* 注册一个视频输入模块, 例如 v4l2 模块 */
int RegisterVideoOpr(PT_VideoOpr ptVideoOpr)
{
	PT_VideoOpr ptTmp;

	/* 管理器使用单链表保存所有视频模块。
	 * main.c 不直接依赖某个具体实现, 而是通过 T_VideoOpr 的函数指针调用。
	 */
	if (!g_ptVideoOprHead)
	{
		g_ptVideoOprHead   = ptVideoOpr;
		ptVideoOpr->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptVideoOprHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext     = ptVideoOpr;
		ptVideoOpr->ptNext = NULL;
	}

	return 0;
}


/* 显示本程序已经注册的视频输入模块 */
void ShowVideoOpr(void)
{
	int i = 0;
	PT_VideoOpr ptTmp = g_ptVideoOprHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/* 根据名字取出指定的视频输入模块 */
PT_VideoOpr GetVideoOpr(char *pcName)
{
	PT_VideoOpr ptTmp = g_ptVideoOprHead;
	
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

int VideoDeviceInit(char *strDevName, PT_VideoDevice ptVideoDevice)
{
    int iError;
	PT_VideoOpr ptTmp = g_ptVideoOprHead;
	
    /* 依次尝试已注册的视频模块。
     * 当前项目只有 v4l2, 但这种结构允许以后扩展 file/net/fake video 输入。
     */
	while (ptTmp)
	{
        iError = ptTmp->InitDevice(strDevName, ptVideoDevice);
        if (!iError)
        {
            return 0;
        }
		ptTmp = ptTmp->ptNext;
	}
    return -1;
}

/* 初始化视频子系统, 注册当前支持的视频输入模块 */
int VideoInit(void)
{
	int iError;

    iError = V4l2Init();

	return iError;
}


