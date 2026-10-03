#ifndef MMI_MOVEORDER_H
#define MMI_MOVEORDER_H

#include "board/movegen.h"

/*
 * Scores every move: TT move first, then captures that do not lose material (by MVV-LVA) and queen
 * promotions, then quiets, then losing captures.
 */
void mmi_order_score(const MmiPosition *pos, const MmiMoveList *list, int scores[MMI_MAX_MOVES], MmiMove tt_move);
/* Moves the best remaining move to index i (selection sort step) and returns it. */
MmiMove mmi_order_pick(MmiMoveList *list, int scores[MMI_MAX_MOVES], int i);

#endif
