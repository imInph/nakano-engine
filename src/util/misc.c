#define _POSIX_C_SOURCE 200809L

#include "util/misc.h"

#ifdef _WIN32
#include <windows.h>

int64_t mmi_now_ms(void) { return (int64_t)GetTickCount64(); }

void mmi_sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <time.h>

int64_t mmi_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void mmi_sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
}
#endif
