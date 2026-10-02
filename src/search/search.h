#ifndef MMI_SEARCH_H
#define MMI_SEARCH_H

#include "board/position.h"
#include "search/timeman.h"

void mmi_search_set_move_overhead(int ms);

/*
 * Starts searching a copy of pos on the search thread. Prints UCI "info" lines
 * and finally "bestmove". Call mmi_search_wait before starting another search.
 */
void mmi_search_start(const MmiPosition *pos, const MmiLimits *limits);
/* Asks a running search to stop; it still prints its best move. */
void mmi_search_stop(void);
/* Blocks until the current search, if any, has finished. */
void mmi_search_wait(void);

/* Searches pos to a fixed depth on the calling thread without output. Returns the node count. */
uint64_t mmi_search_fixed_depth(const MmiPosition *pos, int depth);

#endif
