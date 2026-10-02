#define _POSIX_C_SOURCE 200809L

#include "util/thread.h"

#define MMI_THREAD_STACK (8u * 1024 * 1024)

#ifdef _WIN32
static DWORD WINAPI trampoline(LPVOID p) {
    MmiThread *thread = p;
    thread->fn(thread->arg);
    return 0;
}

bool mmi_thread_start(MmiThread *thread, MmiThreadFn fn, void *arg) {
    thread->fn = fn;
    thread->arg = arg;
    thread->handle = CreateThread(NULL, MMI_THREAD_STACK, trampoline, thread, 0, NULL);
    return thread->handle != NULL;
}

void mmi_thread_join(MmiThread *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
}
#else
static void *trampoline(void *p) {
    MmiThread *thread = p;
    thread->fn(thread->arg);
    return NULL;
}

bool mmi_thread_start(MmiThread *thread, MmiThreadFn fn, void *arg) {
    pthread_attr_t attr;
    thread->fn = fn;
    thread->arg = arg;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, MMI_THREAD_STACK);
    bool ok = pthread_create(&thread->handle, &attr, trampoline, thread) == 0;
    pthread_attr_destroy(&attr);
    return ok;
}

void mmi_thread_join(MmiThread *thread) { pthread_join(thread->handle, NULL); }
#endif
