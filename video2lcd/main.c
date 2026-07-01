#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <string.h>
#include <limits.h>

#include <pthread.h>
#include <frame_queue.h>
#include <thread_context.h>
#include <capture_thread.h>
#include <display_thread.h>

typedef enum RunMode {
    /* 默认实时采集显示流程 */
    MODE_NORMAL = 0,
    /* 查询摄像头和 framebuffer 信息后退出 */
    MODE_QUERY,
    /* 用户显式指定 device/width/height/format 后运行 */
    MODE_CONFIG_RUN
} E_RunMode;

typedef struct VideoConfig {
    /* 用户请求的摄像头参数; pixel_format 是 V4L2 FourCC, format_name 只用于显示 */
    char dev_name[128];
    int width;
    int height;
    char format_name[16];
    int pixel_format;
} T_VideoConfig;

typedef struct VideoAppConfig {
    /* mode 决定 main.c 走查询、默认显示还是配置运行流程 */
    enum RunMode mode;
   T_VideoConfig videoconfig;
} T_VideoAppConfig;

void InitAppConfig(T_VideoAppConfig *videoappconfig)
{
    memset(videoappconfig, 0, sizeof(*videoappconfig));
    videoappconfig->mode = MODE_NORMAL;
}

static void PixelFormatToFourcc(int pixel_format, char fourcc[5])
{
    /* V4L2 像素格式是 FourCC, 例如 0x56595559 对应 "YUYV" */
    fourcc[0] = pixel_format & 0xff;
    fourcc[1] = (pixel_format >> 8) & 0xff;
    fourcc[2] = (pixel_format >> 16) & 0xff;
    fourcc[3] = (pixel_format >> 24) & 0xff;
    fourcc[4] = '\0';
}

int ParseArgs(int argc, char **argv, T_VideoAppConfig *videoappconfig)
{
    int i = 1;
    long value;
    char *endptr;

    if (argc < 2)
    {
        printf("Usage: /dev/video0\n");
        printf("--query /dev/video0\n");
        printf("--device /dev/videoX --width W --height H --format FMT\n");
        printf("Supported formats: YUYV, MJPG\n");
        return -1;
    }

    if (argc == 2 && strncmp(argv[1], "/dev/video", strlen("/dev/video")) == 0)
    {
        videoappconfig->mode = MODE_NORMAL;
        snprintf(videoappconfig->videoconfig.dev_name,
                 sizeof(videoappconfig->videoconfig.dev_name), "%s", argv[1]);
        return 0;
    }

    while (i < argc)
    {
        if (strcmp(argv[i], "--query") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("missing argument for --query\n");
                return -1;
            }
            videoappconfig->mode = MODE_QUERY;
            snprintf(videoappconfig->videoconfig.dev_name,
                     sizeof(videoappconfig->videoconfig.dev_name), "%s", argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--device") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("missing argument for --device\n");
                return -1;
            }
            videoappconfig->mode = MODE_CONFIG_RUN;
            snprintf(videoappconfig->videoconfig.dev_name,
                     sizeof(videoappconfig->videoconfig.dev_name), "%s", argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--width") == 0 ||
                 strcmp(argv[i], "--height") == 0)
        {
            int is_width = strcmp(argv[i], "--width") == 0;

            if (i + 1 >= argc)
            {
                printf("missing argument for %s\n", argv[i]);
                return -1;
            }
            value = strtol(argv[i + 1], &endptr, 10);
            if (*argv[i + 1] == '\0' || *endptr != '\0' ||
                value <= 0 || value > INT_MAX)
            {
                printf("invalid %s: %s\n", is_width ? "width" : "height", argv[i + 1]);
                return -1;
            }
            if (is_width)
                videoappconfig->videoconfig.width = (int)value;
            else
                videoappconfig->videoconfig.height = (int)value;
            i += 2;
        }
        else if (strcmp(argv[i], "--format") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("missing argument for --format\n");
                return -1;
            }
            if (strcmp(argv[i + 1], "YUYV") == 0)
            {
                snprintf(videoappconfig->videoconfig.format_name,
                         sizeof(videoappconfig->videoconfig.format_name), "%s", "YUYV");
                videoappconfig->videoconfig.pixel_format = V4L2_PIX_FMT_YUYV;
            }
            else if (strcmp(argv[i + 1], "MJPG") == 0)
            {
                snprintf(videoappconfig->videoconfig.format_name,
                         sizeof(videoappconfig->videoconfig.format_name), "%s", "MJPG");
                videoappconfig->videoconfig.pixel_format = V4L2_PIX_FMT_MJPEG;
            }
            else
            {
                printf("unsupported format: %s\n", argv[i + 1]);
                return -1;
            }
            i += 2;
        }
        else
        {
            printf("unknown argument: %s\n", argv[i]);
            return -1;
        }
    }

    if (videoappconfig->mode == MODE_CONFIG_RUN &&
        (!videoappconfig->videoconfig.dev_name[0] ||
         videoappconfig->videoconfig.width <= 0 ||
         videoappconfig->videoconfig.height <= 0 ||
         videoappconfig->videoconfig.pixel_format == 0))
    {
        printf("incomplete config: require --device --width --height --format\n");
        return -1;
    }

    return videoappconfig->videoconfig.dev_name[0] ? 0 : -1;
}

void PrintAppConfig(T_VideoAppConfig *videoappconfig)
{
    const char *mode_name = "UNKNOWN";

    if (videoappconfig->mode == MODE_NORMAL)
    {
        mode_name = "NORMAL";
    }
    else if (videoappconfig->mode == MODE_QUERY)
    {
        mode_name = "QUERY";
    }
    else if (videoappconfig->mode == MODE_CONFIG_RUN)
    {
        mode_name = "CONFIG_RUN";
    }

    printf("Run Mode : %s\n", mode_name);
    printf("Device   : %s\n", videoappconfig->videoconfig.dev_name);
    printf("Width    : %d\n", videoappconfig->videoconfig.width);
    printf("Height   : %d\n", videoappconfig->videoconfig.height);
    printf("Format   : %s\n", videoappconfig->videoconfig.format_name);
    printf("PixFmt   : 0x%08x\n", videoappconfig->videoconfig.pixel_format);
}

 /* 用户输入格式
    ./video2lcd /dev/video0
    ./video2lcd --query /dev/video1
    ./video2lcd --device /dev/video1 --width 640 --height 480 --format YUYV
*/
int main(int argc, char **argv)
{
    int iError;
    T_VideoDevice tVideoDevice;
    PT_VideoConvert ptVideoConvert;
    int iPixelFormatOfVideo;
    int iPixelFormatOfDisp;
    T_VideoBuf tFrameBuf;
    T_FrameQueue display_queue;
    T_CaptureContext capture_ctx;
    T_DisplayContext display_ctx;
    pthread_t capture_tid;
    pthread_t display_tid;
    T_VideoAppConfig tVideoAppConfig;

    memset(&tVideoDevice, 0, sizeof(tVideoDevice));
    memset(&display_queue, 0, sizeof(display_queue));
    memset(&capture_ctx, 0, sizeof(capture_ctx));
    memset(&display_ctx, 0, sizeof(display_ctx));

    InitAppConfig(&tVideoAppConfig);
    if (ParseArgs(argc, argv, &tVideoAppConfig) != 0)
        return -1;

    if (tVideoAppConfig.mode == MODE_QUERY)
    {
        /* 查询模式不启动真实采集流程。 */
        return V4l2PrintQuery(tVideoAppConfig.videoconfig.dev_name);
    }

    if (tVideoAppConfig.mode == MODE_CONFIG_RUN)
    {
        PrintAppConfig(&tVideoAppConfig);
        tVideoDevice.iWidth = tVideoAppConfig.videoconfig.width;
        tVideoDevice.iHeight = tVideoAppConfig.videoconfig.height;
        tVideoDevice.iPixelFormat = tVideoAppConfig.videoconfig.pixel_format;
    }

    DisplayInit();
    SelectAndInitDefaultDispDev("fb");
    GetVideoBufForDisplay(&tFrameBuf);
    iPixelFormatOfDisp = tFrameBuf.iPixelFormat;

    VideoInit();
    iError = VideoDeviceInit(tVideoAppConfig.videoconfig.dev_name, &tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("VideoDeviceInit for %s error!\n",
                   tVideoAppConfig.videoconfig.dev_name);
        return -1;
    }

    iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);

    VideoConvertInit();
    ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo,
                                               iPixelFormatOfDisp);
    if (!ptVideoConvert)
    {
        char acVideoFormat[5];
        char acDispFormat[5];

        PixelFormatToFourcc(iPixelFormatOfVideo, acVideoFormat);
        PixelFormatToFourcc(iPixelFormatOfDisp, acDispFormat);
        DBG_PRINTF("can not support this format convert: video=0x%08x(%s), disp=0x%08x(%s)\n",
                   iPixelFormatOfVideo, acVideoFormat,
                   iPixelFormatOfDisp, acDispFormat);
        tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);
        return -1;
    }

    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n",
                   tVideoAppConfig.videoconfig.dev_name);
        tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);
        return -1;
    }

    iError = FrameQueueInit(&display_queue);
    if (iError != FRAME_QUEUE_OK)
    {
        DBG_PRINTF("FrameQueueInit failed, ret=%d\n", iError);
        tVideoDevice.ptOPr->StopDevice(&tVideoDevice);
        tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);
        return -1;
    }

    capture_ctx.video_device = &tVideoDevice;
    capture_ctx.display_queue = &display_queue;

    display_ctx.display_queue = &display_queue;
    display_ctx.video_convert = ptVideoConvert;
    display_ctx.frame_buf = &tFrameBuf;

    iError = pthread_create(&display_tid, NULL, DisplayThread, &display_ctx);
    if (iError)
    {
        DBG_PRINTF("pthread_create DisplayThread failed, ret=%d\n", iError);
        FrameQueueDestroy(&display_queue);
        tVideoDevice.ptOPr->StopDevice(&tVideoDevice);
        tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);
        return -1;
    }

    iError = pthread_create(&capture_tid, NULL, CaptureThread, &capture_ctx);
    if (iError)
    {
        DBG_PRINTF("pthread_create CaptureThread failed, ret=%d\n", iError);
        FrameQueueStop(&display_queue);
        pthread_join(display_tid, NULL);
        FrameQueueDestroy(&display_queue);
        tVideoDevice.ptOPr->StopDevice(&tVideoDevice);
        tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);
        return -1;
    }

    /* 正常情况下采集线程持续运行；异常退出时会 Stop 队列唤醒显示线程。 */
    pthread_join(capture_tid, NULL);
    FrameQueueStop(&display_queue);
    pthread_join(display_tid, NULL);

    FrameQueueDestroy(&display_queue);
    tVideoDevice.ptOPr->StopDevice(&tVideoDevice);
    tVideoDevice.ptOPr->ExitDevice(&tVideoDevice);

    return 0;
}