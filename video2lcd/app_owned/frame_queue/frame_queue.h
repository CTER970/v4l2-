#ifndef _FRAME_QUEUE_H
#define _FRAME_QUEUE_H

#include <pthread.h>
#include <app_owned_frame.h>

#define FRAME_QUEUE_CAPACITY 4

typedef enum FrameQueueResult {
    FRAME_QUEUE_OK = 0,
    FRAME_QUEUE_FULL = -1,
    FRAME_QUEUE_STOPPED = -2,
    FRAME_QUEUE_INVALID = -3,
    FRAME_QUEUE_INIT_FAILED = -4
} E_FrameQueueResult;

typedef struct FrameQueue {
    T_AppOwnedFrame *items[FRAME_QUEUE_CAPACITY];

    int head;   /* 下一次 pop 的位置 */
    int tail;   /* 下一次 push 的位置 */
    int count;  /* 当前保存的帧数 */

    int stopped;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} T_FrameQueue;

int FrameQueueInit(T_FrameQueue *queue);

/*
 * 非阻塞入队：成功后 frame 所有权转移给队列；
 * 失败时 frame 仍由调用者负责销毁。
 */
int FrameQueuePush(
    T_FrameQueue *queue,
    T_AppOwnedFrame *frame);

/* 成功出队后，帧所有权转移给调用者。队列为空时阻塞，Stop 后返回。 */
int FrameQueuePop(
    T_FrameQueue *queue,
    T_AppOwnedFrame **out_frame);

void FrameQueueStop(T_FrameQueue *queue);

/* 必须在所有使用队列的线程退出后调用。函数会释放队列中残留的帧。 */
void FrameQueueDestroy(T_FrameQueue *queue);

#endif /* _FRAME_QUEUE_H */
