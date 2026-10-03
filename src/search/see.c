#include "search/see.h"

/* Exchange values. They are the search's own, so tuning them never changes the evaluation. */
static const int see_value[7] = {0, 100, 320, 330, 500, 900, 0};

bool mmi_see_ge(const MmiPosition *pos, MmiMove m, int threshold) {
    MmiMoveType type = mmi_move_type(m);
    if (type == MMI_MOVE_CASTLING) return threshold <= 0;

    int from = mmi_move_from(m), to = mmi_move_to(m);
    MmiPieceType moved = mmi_piece_type(mmi_piece_on(pos, from));
    int gain = type == MMI_MOVE_EN_PASSANT ? see_value[MMI_PAWN] : see_value[mmi_piece_type(mmi_piece_on(pos, to))];
    if (type == MMI_MOVE_PROMOTION) {
        gain += see_value[mmi_move_promotion(m)] - see_value[MMI_PAWN];
        moved = mmi_move_promotion(m);
    }

    /* swap is what the side to move stands to win (res 1) or lose (res 0) after each capture, minus threshold. */
    int swap = gain - threshold;
    if (swap < 0) return false;
    swap = see_value[moved] - swap;
    /* Even losing the moved piece keeps the threshold; a legal king move can never be recaptured. */
    if (swap <= 0) return true;

    MmiBitboard occ = (mmi_occupied(pos) & ~mmi_square_bb(from)) | mmi_square_bb(to);
    if (type == MMI_MOVE_EN_PASSANT) occ &= ~mmi_square_bb(to ^ 8);
    MmiBitboard diagonal = pos->by_type[MMI_BISHOP] | pos->by_type[MMI_QUEEN];
    MmiBitboard straight = pos->by_type[MMI_ROOK] | pos->by_type[MMI_QUEEN];
    MmiBitboard attackers = mmi_position_attackers_to(pos, to, occ);
    MmiColor stm = pos->side;
    int res = 1;

    for (;;) {
        stm = !stm;
        attackers &= occ;
        MmiBitboard ours = attackers & pos->by_color[stm];
        if (!ours) break;
        res ^= 1;

        /* Capture with the least valuable attacker; removing it may uncover a slider behind it. */
        MmiBitboard b;
        if ((b = ours & pos->by_type[MMI_PAWN])) {
            if ((swap = see_value[MMI_PAWN] - swap) < res) break;
            occ ^= b & -b;
            attackers |= mmi_bishop_attacks(to, occ) & diagonal;
        } else if ((b = ours & pos->by_type[MMI_KNIGHT])) {
            if ((swap = see_value[MMI_KNIGHT] - swap) < res) break;
            occ ^= b & -b;
        } else if ((b = ours & pos->by_type[MMI_BISHOP])) {
            if ((swap = see_value[MMI_BISHOP] - swap) < res) break;
            occ ^= b & -b;
            attackers |= mmi_bishop_attacks(to, occ) & diagonal;
        } else if ((b = ours & pos->by_type[MMI_ROOK])) {
            if ((swap = see_value[MMI_ROOK] - swap) < res) break;
            occ ^= b & -b;
            attackers |= mmi_rook_attacks(to, occ) & straight;
        } else if ((b = ours & pos->by_type[MMI_QUEEN])) {
            if ((swap = see_value[MMI_QUEEN] - swap) < res) break;
            occ ^= b & -b;
            attackers |= (mmi_bishop_attacks(to, occ) & diagonal) | (mmi_rook_attacks(to, occ) & straight);
        } else {
            /* Only the king is left: it may recapture only if the square is no longer defended. */
            return (attackers & pos->by_color[!stm]) ? res ^ 1 : res;
        }
    }
    return res;
}
