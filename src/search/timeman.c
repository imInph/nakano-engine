#include "search/timeman.h"

#include "util/misc.h"

static int64_t min64(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t max64(int64_t a, int64_t b) { return a > b ? a : b; }

void mmi_limits_clear(MmiLimits *limits) { *limits = (MmiLimits){.time = {-1, -1}}; }

void mmi_time_init(MmiTimeManager *tm, const MmiLimits *limits, MmiColor us, int64_t move_overhead) {
    tm->start = mmi_now_ms();
    tm->optimum = tm->maximum = -1;
    if (limits->infinite) return;

    /* A fixed move time is all hard limit: there is no clock to save time for. */
    if (limits->movetime > 0) {
        tm->maximum = max64(1, limits->movetime - move_overhead);
        return;
    }
    if (limits->time[us] < 0) return;

    int64_t available = max64(1, limits->time[us] - move_overhead);
    int64_t inc = limits->inc[us];
    /* Without movestogo, plan for 40 more moves: games rarely last that long, so the clock lasts to the end. */
    int moves_to_go = limits->movestogo > 0 ? (limits->movestogo < 50 ? limits->movestogo : 50) : 40;

    /*
     * Never plan more than a quarter of the clock for one move, nor allow more than half, so a low clock is
     * not spent down to the increment, where any delay in the GUI or the operating system loses on time.
     */
    tm->optimum = max64(1, min64(available / moves_to_go + inc * 3 / 4, available / 4));
    tm->maximum = max64(1, min64(tm->optimum * 3, available / 2));
}

int64_t mmi_time_elapsed(const MmiTimeManager *tm) { return mmi_now_ms() - tm->start; }
