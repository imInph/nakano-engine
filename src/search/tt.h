#ifndef MMI_TT_H
#define MMI_TT_H

#include <stddef.h>

#include "mmi.h"

typedef enum { MMI_BOUND_NONE, MMI_BOUND_UPPER, MMI_BOUND_LOWER, MMI_BOUND_EXACT } MmiBound;

/* A copy of a stored entry. Entries can be overwritten by other threads, so probes never hand out pointers. */
typedef struct {
    MmiValue value; /* as stored: mate scores are relative to the node */
    MmiMove move;
    int depth;
    MmiBound bound;
} MmiTTData;

/* Allocates as many 64-byte clusters as fit in mb megabytes and clears them. */
bool mmi_tt_resize(size_t mb);
void mmi_tt_clear(void);
void mmi_tt_free(void);

/* Ages the table. Call once per search, before it starts. */
void mmi_tt_new_search(void);

/* Returns true and fills *out if key is stored. */
bool mmi_tt_probe(MmiKey key, MmiTTData *out);
void mmi_tt_store(MmiKey key, MmiValue value, MmiMove move, int depth, MmiBound bound);
/* Permille of the entries in the first 1000 clusters written during the current search. */
int mmi_tt_hashfull(void);

/* Mate scores are stored relative to the node, not the root. */
static inline MmiValue mmi_value_to_tt(MmiValue v, int ply) {
    return v >= MMI_VALUE_MATE_IN_MAX_PLY ? v + ply : v <= -MMI_VALUE_MATE_IN_MAX_PLY ? v - ply : v;
}
static inline MmiValue mmi_value_from_tt(MmiValue v, int ply) {
    return v >= MMI_VALUE_MATE_IN_MAX_PLY ? v - ply : v <= -MMI_VALUE_MATE_IN_MAX_PLY ? v + ply : v;
}

#endif
