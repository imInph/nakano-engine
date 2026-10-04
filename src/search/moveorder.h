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
 * Continuation history for one earlier move: entries by the piece moved now and its destination, within
 * +-16384. Scores how well a quiet move has followed that earlier move.
 */
typedef struct {
    int score[MMI_PIECE_NB][64];
} MmiPieceToHistory;

/*
 * Scores every move: TT move first, then captures that do not lose material (by MVV-LVA) and queen
 * promotions, then the killers, then other quiets by butterfly plus continuation history (the tables
 * of the moves one and two plies back), then losing captures. killers, history (the side to move's
 * table) and continuation may be NULL.
 */
void mmi_order_score(const MmiPosition *pos, const MmiMoveList *list, int scores[MMI_MAX_MOVES], MmiMove tt_move,
                     const MmiMove killers[2], const MmiButterflyHistory *history,
                     const MmiPieceToHistory *const continuation[2]);
/* Moves the best remaining move to index i (selection sort step) and returns it. */
MmiMove mmi_order_pick(MmiMoveList *list, int scores[MMI_MAX_MOVES], int i);

#endif
