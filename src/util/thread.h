#ifndef MMI_THREAD_H
#define MMI_THREAD_H

#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef void (*MmiThreadFn)(void *arg);

/* The struct must stay alive and unmoved until mmi_thread_join returns. */
typedef struct {
    MmiThreadFn fn;
    void *arg;
#ifdef _WIN32
    HANDLE handle;
#else
    pthread_t handle;
#endif
} MmiThread;

/* Starts fn(arg) on a new thread with an 8 MB stack. */
bool mmi_thread_start(MmiThread *thread, MmiThreadFn fn, void *arg);
void mmi_thread_join(MmiThread *thread);

#endif
