#ifndef _DISPLAY_THREAD_H
#define _DISPLAY_THREAD_H

/* pthread入口；显示资源加入T_ThreadContext后实现完整消费流程。 */
void *DisplayThread(void *arg);

#endif /* _DISPLAY_THREAD_H */
