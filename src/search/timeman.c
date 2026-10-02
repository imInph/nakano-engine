#include "search/timeman.h"

#include "util/misc.h"

static int64_t min64(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t max64(int64_t a, int64_t b) { return a > b ? a : b; }

void mmi_limits_clear(MmiLimits *limits) {
    *limits = (MmiLimits){{-1, -1}, {0, 0}, 0, 0, 0, 0, false};
}

void mmi_time_init(MmiTimeManager *tm, const MmiLimits *limits, MmiColor us, int64_t move_overhead) {
    tm->start = mmi_now_ms();
    tm->optimum = tm->maximum = -1;
    if (limits->infinite) return;

    if (limits->movetime > 0) {
        tm->optimum = tm->maximum = max64(1, limits->movetime - move_overhead);
        return;
    }
    if (limits->time[us] < 0) return;

    int64_t available = max64(1, limits->time[us] - move_overhead);
    int64_t inc = limits->inc[us];
    int moves_to_go = limits->movestogo > 0 ? (limits->movestogo < 40 ? limits->movestogo : 40) : 20;

    tm->optimum = max64(1, min64(available / moves_to_go + inc * 3 / 4, available * 6 / 10));
    tm->maximum = max64(1, min64(tm->optimum * 4, available * 8 / 10));
}

int64_t mmi_time_elapsed(const MmiTimeManager *tm) { return mmi_now_ms() - tm->start; }
