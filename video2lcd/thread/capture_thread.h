#ifndef _CAPTURE_THREAD_H
#define _CAPTURE_THREAD_H

/* pthread入口: arg必须指向在线程退出前持续有效的T_CaptureContext。 */
void *CaptureThread(void *arg);

#endif /* _CAPTURE_THREAD_H */
