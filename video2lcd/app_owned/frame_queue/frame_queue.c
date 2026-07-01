#include "frame_queue.h"

int FrameQueueInit(T_FrameQueue *queue)
{
    int i;
    int ret;

    if (!queue)
        return FRAME_QUEUE_INVALID;

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->stopped = 0;

    for (i = 0; i < FRAME_QUEUE_CAPACITY; i++)
        queue->items[i] = NULL;

    ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret != 0)
        return FRAME_QUEUE_INIT_FAILED;

    ret = pthread_cond_init(&queue->not_empty, NULL);
    if (ret != 0)
    {
        /* 条件变量初始化失败，回收已经初始化的 mutex。 */
        pthread_mutex_destroy(&queue->mutex);
        return FRAME_QUEUE_INIT_FAILED;
    }

    return FRAME_QUEUE_OK;
}

int FrameQueuePush(T_FrameQueue *queue, T_AppOwnedFrame *frame)
{
    if (!queue || !frame)
        return FRAME_QUEUE_INVALID;

    pthread_mutex_lock(&queue->mutex);

    if (queue->stopped)
    {
        pthread_mutex_unlock(&queue->mutex);
        return FRAME_QUEUE_STOPPED;
    }

    if (queue->count == FRAME_QUEUE_CAPACITY)
    {
        pthread_mutex_unlock(&queue->mutex);
        return FRAME_QUEUE_FULL;
    }

    queue->items[queue->tail] = frame;
    queue->tail = (queue->tail + 1) % FRAME_QUEUE_CAPACITY;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return FRAME_QUEUE_OK;
}

int FrameQueuePop(T_FrameQueue *queue, T_AppOwnedFrame **out_frame)
{
    if (!queue || !out_frame)
        return FRAME_QUEUE_INVALID;

    *out_frame = NULL;

    pthread_mutex_lock(&queue->mutex);

    /* while 可以正确处理条件变量的虚假唤醒。 */
    while (queue->count == 0 && !queue->stopped)
        pthread_cond_wait(&queue->not_empty, &queue->mutex);

    /* Stop 后仍允许消费者取完队列中的剩余帧。 */
    if (queue->count == 0)
    {
        pthread_mutex_unlock(&queue->mutex);
        return FRAME_QUEUE_STOPPED;
    }

    *out_frame = queue->items[queue->head];
    queue->items[queue->head] = NULL;
    queue->head = (queue->head + 1) % FRAME_QUEUE_CAPACITY;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);
    return FRAME_QUEUE_OK;
}

void FrameQueueStop(T_FrameQueue *queue)
{
    if (!queue)
        return;

    pthread_mutex_lock(&queue->mutex);

    /* 标记队列停止，不再接受新帧。 */
    queue->stopped = 1;

    /* 唤醒所有正在等待队列非空的线程。 */
    pthread_cond_broadcast(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);
}

void FrameQueueDestroy(T_FrameQueue *queue)
{
    int i;

    if (!queue)
        return;

    /* 调用者必须先 Stop 并 join 所有线程，Destroy 不负责并发退出。 */
    for (i = 0; i < FRAME_QUEUE_CAPACITY; i++)
    {
        if (queue->items[i])
        {
            AppOwnedFrameDestroy(queue->items[i]);
            queue->items[i] = NULL;
        }
    }

    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
}
