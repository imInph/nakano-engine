#ifndef MMI_MOVEORDER_H
#define MMI_MOVEORDER_H

#include "board/movegen.h"

/*
 * One side's butterfly history by from and to square, entries within +-16384. Wrapped in a struct so it
 * can be passed as const, which a plain two-dimensional array cannot be in C17.
 */
typedef struct {
    int score[64][64];
} MmiButterflyHistory;

/*
 * Scores every move: TT move first, then captures that do not lose material (by MVV-LVA) and queen
 * promotions, then the killers, then other quiets by history, then losing captures. killers and
 * history (the side to move's table) may be NULL.
 */
void mmi_order_score(const MmiPosition *pos, const MmiMoveList *list, int scores[MMI_MAX_MOVES], MmiMove tt_move,
                     const MmiMove killers[2], const MmiButterflyHistory *history);
/* Moves the best remaining move to index i (selection sort step) and returns it. */
MmiMove mmi_order_pick(MmiMoveList *list, int scores[MMI_MAX_MOVES], int i);

#endif
