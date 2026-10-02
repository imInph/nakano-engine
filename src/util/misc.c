#include "util/misc.h"

#ifdef _WIN32
#include <windows.h>

int64_t mmi_now_ms(void) { return (int64_t)GetTickCount64(); }
#else
#include <time.h>

int64_t mmi_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif
