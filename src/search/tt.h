#ifndef MMI_TT_H
#define MMI_TT_H

#include <stddef.h>

#include "mmi.h"

typedef enum { MMI_BOUND_NONE, MMI_BOUND_UPPER, MMI_BOUND_LOWER, MMI_BOUND_EXACT } MmiBound;

typedef struct {
    MmiKey key;
    int16_t value;
    MmiMove move;
    int8_t depth;
    uint8_t bound;
} MmiTTEntry;

/* Allocates the largest power-of-two table that fits in mb megabytes and clears it. */
bool mmi_tt_resize(size_t mb);
void mmi_tt_clear(void);
void mmi_tt_free(void);

/* Returns the entry for key, or NULL if it is not stored. Values are as stored (mate scores ply-adjusted). */
const MmiTTEntry *mmi_tt_probe(MmiKey key);
void mmi_tt_store(MmiKey key, MmiValue value, MmiMove move, int depth, MmiBound bound);
/* Permille of the first 1000 entries in use. */
int mmi_tt_hashfull(void);

/* Mate scores are stored relative to the node, not the root. */
static inline MmiValue mmi_value_to_tt(MmiValue v, int ply) {
    return v >= MMI_VALUE_MATE_IN_MAX_PLY ? v + ply : v <= -MMI_VALUE_MATE_IN_MAX_PLY ? v - ply : v;
}
static inline MmiValue mmi_value_from_tt(MmiValue v, int ply) {
    return v >= MMI_VALUE_MATE_IN_MAX_PLY ? v - ply : v <= -MMI_VALUE_MATE_IN_MAX_PLY ? v + ply : v;
}

#endif
