#include "search/moveorder.h"

void mmi_order_score(const MmiPosition *pos, const MmiMoveList *list, int scores[MMI_MAX_MOVES], MmiMove tt_move) {
    for (int i = 0; i < list->count; i++) {
        MmiMove m = list->moves[i];
        int score = 0;
        if (m == tt_move) {
            score = 2000000;
        } else if (mmi_is_capture(pos, m)) {
            MmiPieceType victim =
                mmi_move_type(m) == MMI_MOVE_EN_PASSANT ? MMI_PAWN : mmi_piece_type(mmi_piece_on(pos, mmi_move_to(m)));
            MmiPieceType attacker = mmi_piece_type(mmi_piece_on(pos, mmi_move_from(m)));
            score = 1000000 + 10 * victim - attacker;
        }
        if (mmi_move_type(m) == MMI_MOVE_PROMOTION && m != tt_move)
            score += mmi_move_promotion(m) == MMI_QUEEN ? 1000000 : -1000;
        scores[i] = score;
    }
}

MmiMove mmi_order_pick(MmiMoveList *list, int scores[MMI_MAX_MOVES], int i) {
    int best = i;
    for (int j = i + 1; j < list->count; j++)
        if (scores[j] > scores[best]) best = j;
    MmiMove m = list->moves[best];
    int s = scores[best];
    list->moves[best] = list->moves[i];
    scores[best] = scores[i];
    list->moves[i] = m;
    scores[i] = s;
    return m;
}
