#include <config.h>
#include <video_manager.h>
#include <disp_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* 支持的像素格式：YUYV(16bit打包)、MJPEG(压缩)、RGB565 */
static int g_aiSupportedFormats[] = {V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUYV,  V4L2_PIX_FMT_RGB565};
/* V4L2操作函数表，通过V4l2Init注册到上层，GetFrame/PutFrame会根据设备能力动态切换 */
static T_VideoOpr g_tV4l2VideoOpr;

static int V4l2GetFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);
static int V4l2PutFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf);

/* 检查格式是否被支持：用于V4l2InitDevice中枚举格式时筛选 */
static int isSupportThisFormat(int iPixelFormat)
{
    int i;
    for (i = 0; i < sizeof(g_aiSupportedFormats)/sizeof(g_aiSupportedFormats[0]); i++)
    {
        if (g_aiSupportedFormats[i] == iPixelFormat)
            return 1;
    }
    return 0;
}

static unsigned int V4l2GetEffectiveCaps(const struct v4l2_capability *cap)
{
    if (cap->capabilities & V4L2_CAP_DEVICE_CAPS)
        return cap->device_caps;

    return cap->capabilities;
}

/* V4l2InitDevice - V4L2设备初始化，根据设备能力动态选择GetFrame/PutFrame实现
 * 核心流程: open → QUERYCAP → ENUM_FMT → S_FMT → REQBUFS；
 * Streaming: QUERYBUF(mmap) → QBUF → STREAMON → poll/DQBUF；ReadWrite: 直接read */
static int V4l2InitDevice(char *strDevName, PT_VideoDevice ptVideoDevice)
{
    int i;
    int iFd;
    int iError;
    struct v4l2_capability tV4l2Cap;
	struct v4l2_fmtdesc tFmtDesc;
    struct v4l2_format  tV4l2Fmt;
    struct v4l2_requestbuffers tV4l2ReqBuffs;
    struct v4l2_buffer tV4l2Buf;

    int iLcdWidth;
    int iLcdHeigt;
    int iLcdBpp;

    int iReqWidth = ptVideoDevice->iWidth;
    int iReqHeight = ptVideoDevice->iHeight;
    int iReqPixelFormat = ptVideoDevice->iPixelFormat;
    unsigned int iEffectiveCaps;


    iFd = open(strDevName, O_RDWR);
    if (iFd < 0)
    {
        DBG_PRINTF("open %s failed: %s\n", strDevName, strerror(errno));
        return -1;
    }
    ptVideoDevice->iFd = iFd;

    memset(&tV4l2Cap, 0, sizeof(struct v4l2_capability));
    iError = ioctl(iFd, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError) {
    	DBG_PRINTF("Error opening device %s: unable to query device.\n", strDevName);
    	goto err_exit;
    }
    iEffectiveCaps = V4l2GetEffectiveCaps(&tV4l2Cap);

    if (!(iEffectiveCaps & V4L2_CAP_VIDEO_CAPTURE))
    {
    	DBG_PRINTF("%s is not a single-plane video capture device, effective_caps=0x%08x\n",
                   strDevName, iEffectiveCaps);
        goto err_exit;
    }

	if (iEffectiveCaps & V4L2_CAP_STREAMING) {
	    DBG_PRINTF("%s supports streaming i/o\n", strDevName);
	}
    else
    {
        DBG_PRINTF("%s does not support streaming i/o\n", strDevName);
        goto err_exit;
    }

	if (iEffectiveCaps & V4L2_CAP_READWRITE) {
	    DBG_PRINTF("%s supports read i/o\n", strDevName);
	}

	/* 按程序优先级选择格式：先判断用户是否传参格式，没有则遍历我们的数组，看驱动是否支持 */


    if (iReqPixelFormat != 0)
    {
    ptVideoDevice->iPixelFormat = iReqPixelFormat;
    }
    else
    {
        for (i = 0; i < sizeof(g_aiSupportedFormats)/sizeof(g_aiSupportedFormats[0]); i++)
	{
		memset(&tFmtDesc, 0, sizeof(tFmtDesc));
		tFmtDesc.index = 0;
		tFmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		while ((iError = ioctl(iFd, VIDIOC_ENUM_FMT, &tFmtDesc)) == 0)
		{
			if (tFmtDesc.pixelformat == g_aiSupportedFormats[i])
			{
				ptVideoDevice->iPixelFormat = g_aiSupportedFormats[i];
				break;
			}
			tFmtDesc.index++;
		}

		if (ptVideoDevice->iPixelFormat)
			break;
	}

    if (!ptVideoDevice->iPixelFormat)
    {
    	DBG_PRINTF("can not support the format of this device\n");
        goto err_exit;
    }
    }
	


    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);/*获得显示设备的分辨率*/

  
    memset(&tV4l2Fmt, 0, sizeof(struct v4l2_format));
    tV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Fmt.fmt.pix.pixelformat = ptVideoDevice->iPixelFormat;


if (iReqWidth > 0)
{
    tV4l2Fmt.fmt.pix.width = iReqWidth;
}
else
{
    tV4l2Fmt.fmt.pix.width = iLcdWidth;
}

if (iReqHeight > 0)
{
    tV4l2Fmt.fmt.pix.height = iReqHeight;
}
else
{
    tV4l2Fmt.fmt.pix.height = iLcdHeigt;
}

tV4l2Fmt.fmt.pix.field = V4L2_FIELD_ANY;


    /* 某些摄像头不支持某些分辨率,驱动会调整参数并返回给应用程序 */
    iError = ioctl(iFd, VIDIOC_S_FMT, &tV4l2Fmt);
    if (iError)
    {
    	DBG_PRINTF("VIDIOC_S_FMT failed: %s\n", strerror(errno));
        goto err_exit;
    }

    ptVideoDevice->iWidth  = tV4l2Fmt.fmt.pix.width;
    ptVideoDevice->iHeight = tV4l2Fmt.fmt.pix.height;
    ptVideoDevice->iPixelFormat = tV4l2Fmt.fmt.pix.pixelformat;

DBG_PRINTF("actual video format: width=%d, height=%d, pixfmt=0x%08x\n",
           ptVideoDevice->iWidth,
           ptVideoDevice->iHeight,
           ptVideoDevice->iPixelFormat);

    /* buffer 生命周期第 1 步: REQBUFS
     * 向驱动申请一组 MMAP 类型的采集 buffer。
     * 注意: count 是请求数量, ioctl 返回后 count 可能被驱动调整。
     */
    memset(&tV4l2ReqBuffs, 0, sizeof(struct v4l2_requestbuffers));
    tV4l2ReqBuffs.count = NB_BUFFER;
    tV4l2ReqBuffs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2ReqBuffs.memory = V4L2_MEMORY_MMAP;

    iError = ioctl(iFd, VIDIOC_REQBUFS, &tV4l2ReqBuffs);
    if (iError)
    {
    	DBG_PRINTF("Unable to allocate buffers.\n");
        goto err_exit;
    }

    if (tV4l2ReqBuffs.count == 0)
    {
        DBG_PRINTF("driver allocated no capture buffers\n");
        goto err_exit;
    }
    ptVideoDevice->iVideoBufCnt = tV4l2ReqBuffs.count > NB_BUFFER ?
                                  NB_BUFFER : tV4l2ReqBuffs.count;
    DBG_PRINTF("using %d of %u capture buffers\n",
               ptVideoDevice->iVideoBufCnt, tV4l2ReqBuffs.count);
    if (iEffectiveCaps & V4L2_CAP_STREAMING)
    {
        /* QUERYBUF 查询每个 buffer 的 length/offset，mmap 映射到用户空间;
         * mmap 返回地址存 pucVideBuf[i], DQBUF 得到 index 后据此访问帧数据 */
        for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++)
        {
        	memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
        	tV4l2Buf.index = i;
        	tV4l2Buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        	tV4l2Buf.memory = V4L2_MEMORY_MMAP;
        	iError = ioctl(iFd, VIDIOC_QUERYBUF, &tV4l2Buf);
        	if (iError)
            {
        	    DBG_PRINTF("Unable to query buffer.\n");
        	    goto err_exit;
        	}

            ptVideoDevice->iVideoBufMaxLen = tV4l2Buf.length;
            /* 让内核选择用户空间地址; MAP_SHARED 表示和驱动 buffer 共享同一块数据 */
        	ptVideoDevice->pucVideBuf[i] = mmap(0 /* start anywhere */ ,
        			  tV4l2Buf.length, PROT_READ, MAP_SHARED, iFd,
        			  tV4l2Buf.m.offset);
        	if (ptVideoDevice->pucVideBuf[i] == MAP_FAILED)
            {
        	    DBG_PRINTF("Unable to map buffer\n");
        	    goto err_exit;
        	}
        }

        /* 初始化阶段把所有空 buffer 先入队，STREAMON 后驱动才能取空闲块填充图像 */
        for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++)
        {
        	memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
        	tV4l2Buf.index = i;
        	tV4l2Buf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        	tV4l2Buf.memory = V4L2_MEMORY_MMAP;
        	iError = ioctl(iFd, VIDIOC_QBUF, &tV4l2Buf);
        	if (iError)
            {
        	    DBG_PRINTF("Unable to queue buffer.\n");
        	    goto err_exit;
        	}
        }

    }
    else if (tV4l2Cap.capabilities & V4L2_CAP_READWRITE)
    {
        /* 动态切换到ReadWrite模式 */
        g_tV4l2VideoOpr.GetFrame = V4l2GetFrameForReadWrite;
        g_tV4l2VideoOpr.PutFrame = V4l2PutFrameForReadWrite;

        ptVideoDevice->iVideoBufCnt  = 1;
        /* 分配足够空间支持任意格式 */
        ptVideoDevice->iVideoBufMaxLen = ptVideoDevice->iWidth * ptVideoDevice->iHeight * 4;
        ptVideoDevice->pucVideBuf[0] = malloc(ptVideoDevice->iVideoBufMaxLen);
    }

    ptVideoDevice->ptOPr = &g_tV4l2VideoOpr;
    return 0;

err_exit:
    /* 当前初始化失败路径只关闭 fd；部分 mmap 成功后的完整回滚仍待统一清理流程处理。 */
    close(iFd);
    return -1;
}

/* V4l2ExitDevice - 解除内存映射，关闭设备 */
static int V4l2ExitDevice(PT_VideoDevice ptVideoDevice)
{
    int i;
    /* 正常退出路径: 解除所有 mmap buffer 的用户空间映射 */
    for (i = 0; i < ptVideoDevice->iVideoBufCnt; i++)
    {
        if (ptVideoDevice->pucVideBuf[i])
        {
            munmap(ptVideoDevice->pucVideBuf[i], ptVideoDevice->iVideoBufMaxLen);
            ptVideoDevice->pucVideBuf[i] = NULL;
        }
    }

    close(ptVideoDevice->iFd);
    return 0;
}

/* V4l2GetFrameForStreaming - Streaming模式获取视频帧，poll等待后DQBUF出队，须与PutFrame配对 */
static int V4l2GetFrameForStreaming(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    struct pollfd tFds[1];
    int iRet;
    struct v4l2_buffer tV4l2Buf;

    tFds[0].fd     = ptVideoDevice->iFd;
    tFds[0].events = POLLIN;

    iRet = poll(tFds, 1, -1);
    if (iRet <= 0)
    {
        DBG_PRINTF("poll error!\n");
        return -1;
    }

    /* DQBUF 从完成队列取出已填充 buffer: index 指示哪个 mmap buffer, bytesused 为本帧字节数 */
    memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
    tV4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tV4l2Buf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(ptVideoDevice->iFd, VIDIOC_DQBUF, &tV4l2Buf);
    if (iRet < 0)
    {
    	DBG_PRINTF("Unable to dequeue buffer.\n");
    	return -1;
    }
    if (tV4l2Buf.index >= (unsigned int)ptVideoDevice->iVideoBufCnt ||
        tV4l2Buf.index >= NB_BUFFER)
    {
        DBG_PRINTF("invalid dequeued buffer index: %u\n", tV4l2Buf.index);
        return -1;
    }
    ptVideoDevice->iVideoBufCurIndex = tV4l2Buf.index;

    /* 组合 T_VideoBuf: 格式/宽高取自设备实际格式, 大小取 bytesused, 数据地址取 pucVideBuf[index] */
    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    = (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 : \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 0 :  \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565) ? 16 :  \
                                        0;
    ptVideoBuf->tPixelDatas.iLineBytes    = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8;
    ptVideoBuf->tPixelDatas.iTotalBytes   = tV4l2Buf.bytesused;
    ptVideoBuf->tPixelDatas.aucPixelDatas = ptVideoDevice->pucVideBuf[tV4l2Buf.index];
    return 0;
}


/* V4l2PutFrameForStreaming - Streaming模式用QBUF把用完的buffer归还驱动队列，须在GetFrame之后调用 */
static int V4l2PutFrameForStreaming(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    /* 用 GetFrame 中 DQBUF 取出的 iVideoBufCurIndex 把 buffer 还给驱动 */
    struct v4l2_buffer tV4l2Buf;
    int iError;

    if (ptVideoDevice->iVideoBufCurIndex < 0 ||
        ptVideoDevice->iVideoBufCurIndex >= ptVideoDevice->iVideoBufCnt ||
        ptVideoDevice->iVideoBufCurIndex >= NB_BUFFER)
    {
        DBG_PRINTF("invalid buffer index for queue: %d\n",
                   ptVideoDevice->iVideoBufCurIndex);
        return -1;
    }

	memset(&tV4l2Buf, 0, sizeof(struct v4l2_buffer));
	tV4l2Buf.index  = ptVideoDevice->iVideoBufCurIndex;
	tV4l2Buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	tV4l2Buf.memory = V4L2_MEMORY_MMAP;
	iError = ioctl(ptVideoDevice->iFd, VIDIOC_QBUF, &tV4l2Buf);
	if (iError)
    {
	    DBG_PRINTF("Unable to queue buffer.\n");
	    return -1;
	}
    return 0;
}

/* V4l2GetFrameForReadWrite - ReadWrite模式直接read读取帧，仅设备不支持Streaming时使用 */
static int V4l2GetFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    int iRet;

    iRet = read(ptVideoDevice->iFd, ptVideoDevice->pucVideBuf[0], ptVideoDevice->iVideoBufMaxLen);
    if (iRet <= 0)
    {
        return -1;
    }

    ptVideoBuf->iPixelFormat        = ptVideoDevice->iPixelFormat;
    ptVideoBuf->tPixelDatas.iWidth  = ptVideoDevice->iWidth;
    ptVideoBuf->tPixelDatas.iHeight = ptVideoDevice->iHeight;
    ptVideoBuf->tPixelDatas.iBpp    = (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_YUYV) ? 16 : \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_MJPEG) ? 0 :  \
                                        (ptVideoDevice->iPixelFormat == V4L2_PIX_FMT_RGB565)? 16 : \
                                          0;
    ptVideoBuf->tPixelDatas.iLineBytes    = ptVideoDevice->iWidth * ptVideoBuf->tPixelDatas.iBpp / 8;
    ptVideoBuf->tPixelDatas.iTotalBytes   = iRet;
    ptVideoBuf->tPixelDatas.aucPixelDatas = ptVideoDevice->pucVideBuf[0];

    return 0;
}


/* V4l2PutFrameForReadWrite - ReadWrite模式无需归还buffer，空实现 */
static int V4l2PutFrameForReadWrite(PT_VideoDevice ptVideoDevice, PT_VideoBuf ptVideoBuf)
{
    return 0;
}

/* V4l2StartDevice - STREAMON启动采集，驱动从空闲队列取buffer填充后移入完成队列 */
static int V4l2StartDevice(PT_VideoDevice ptVideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    /* 前面必须已 QBUF 一批空 buffer, 驱动才有地方写摄像头数据 */
    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMON, &iType);
    if (iError)
    {
    	DBG_PRINTF("Unable to start capture.\n");
    	return -1;
    }
    return 0;
}

/* V4l2StopDevice - STREAMOFF停止采集 */
static int V4l2StopDevice(PT_VideoDevice ptVideoDevice)
{
    int iType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int iError;

    /* 退出阶段第 1 步: STREAMOFF
     * 先停止驱动采集, 再 munmap/close, 避免驱动仍在写已经释放的 buffer。
     */
    iError = ioctl(ptVideoDevice->iFd, VIDIOC_STREAMOFF, &iType);
    if (iError)
    {
    	DBG_PRINTF("Unable to stop capture.\n");
    	return -1;
    }
    return 0;
}

/* V4l2GetFormat - 获取当前像素格式 */
static int V4l2GetFormat(PT_VideoDevice ptVideoDevice)
{
    return ptVideoDevice->iPixelFormat;
}


/* V4L2操作函数表：默认Streaming模式，设备仅支持ReadWrite则动态切换 */
static T_VideoOpr g_tV4l2VideoOpr = {
    .name        = "v4l2",
    .InitDevice  = V4l2InitDevice,
    .ExitDevice  = V4l2ExitDevice,
    .GetFormat   = V4l2GetFormat,
    .GetFrame    = V4l2GetFrameForStreaming,
    .PutFrame    = V4l2PutFrameForStreaming,
    .StartDevice = V4l2StartDevice,
    .StopDevice  = V4l2StopDevice,
};

/* V4l2Init - 注册V4L2操作结构体到上层管理器 */
int V4l2Init(void)
{
    return RegisterVideoOpr(&g_tV4l2VideoOpr);
}


int V4l2PrintQuery(const char *dev_name)
{
int fd;
struct v4l2_capability tV4l2Cap;
int iError;    
unsigned int iEffectiveCaps;

    fd = open(dev_name, O_RDWR);
    if (fd < 0)
    {
        DBG_PRINTF("can not open %s\n", dev_name);
        return -1;
    }
    
    memset(&tV4l2Cap, 0, sizeof(struct v4l2_capability));
    iError = ioctl(fd, VIDIOC_QUERYCAP, &tV4l2Cap);
    if (iError < 0)
     {
    	DBG_PRINTF("VIDIOC_QUERYCAP failed for %s: %s\n", dev_name, strerror(errno));
    	goto err_exit;
    }
    iEffectiveCaps = V4l2GetEffectiveCaps(&tV4l2Cap);
    printf("driver       : %s\n", tV4l2Cap.driver);
    printf("card         : %s\n", tV4l2Cap.card);
    printf("bus_info     : %s\n", tV4l2Cap.bus_info);
    printf("capabilities : 0x%08x\n", tV4l2Cap.capabilities);
    printf("device_caps  : 0x%08x\n", tV4l2Cap.device_caps);
    printf("effective    : 0x%08x\n", iEffectiveCaps);

    if (iEffectiveCaps & V4L2_CAP_VIDEO_CAPTURE)
    {
    	DBG_PRINTF("%s is  a video capture device\n", dev_name);
    }
    else if (iEffectiveCaps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
    {
        DBG_PRINTF("%s is a multi-plane video capture device (not supported by this program)\n",
                   dev_name);
    }
    else if (iEffectiveCaps & V4L2_CAP_VIDEO_OUTPUT)
    {
        DBG_PRINTF("%s is a video output device, not a capture device\n", dev_name);
    }
    else
    {
        DBG_PRINTF("%s is not a supported video capture device\n", dev_name);
    }

	if (iEffectiveCaps & V4L2_CAP_STREAMING) {
	    DBG_PRINTF("%s supports streaming i/o\n", dev_name);
	}

	if (iEffectiveCaps & V4L2_CAP_READWRITE) {
	    DBG_PRINTF("%s supports read i/o\n", dev_name);
	}

struct v4l2_fmtdesc tFmtDesc;

		memset(&tFmtDesc, 0, sizeof(tFmtDesc));
		tFmtDesc.index = 0;
        if (iEffectiveCaps & V4L2_CAP_VIDEO_CAPTURE)
        {
		    tFmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		    while ((iError = ioctl(fd, VIDIOC_ENUM_FMT, &tFmtDesc)) == 0)
		    {
			    printf("support format is 0x%08x\n",tFmtDesc.pixelformat);
                printf("support format description is %s\n",tFmtDesc.description);
			    tFmtDesc.index++;
            }
        }
    close(fd);
    return 0;
    err_exit:
        close(fd);
        return -1;
}
