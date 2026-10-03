#ifndef MMI_TIMEMAN_H
#define MMI_TIMEMAN_H

#include <stdint.h>

#include "mmi.h"

/* What "go" asked for. Times are in milliseconds; -1 or 0 means not given. */
typedef struct {
    int64_t time[2];
    int64_t inc[2];
    int64_t movetime;
    int movestogo;
    int depth;
    int mate; /* in moves */
    uint64_t nodes;
    bool infinite;
    bool ponder;
    int searchmoves_count; /* 0: search every root move */
    MmiMove searchmoves[MMI_MAX_MOVES];
} MmiLimits;

typedef struct {
    int64_t start;
    int64_t optimum; /* do not start a new iteration after this; -1 if unlimited */
    int64_t maximum; /* abort the search at this point; -1 if unlimited */
} MmiTimeManager;

void mmi_limits_clear(MmiLimits *limits);
void mmi_time_init(MmiTimeManager *tm, const MmiLimits *limits, MmiColor us, int64_t move_overhead);
int64_t mmi_time_elapsed(const MmiTimeManager *tm);

#endif
