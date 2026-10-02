#include "eval/eval.h"

const MmiValue mmi_piece_value[7] = {0, 100, 320, 330, 500, 900, 0};

MmiValue mmi_evaluate(const MmiPosition *pos) {
    MmiValue score = 0;
    for (MmiPieceType pt = MMI_PAWN; pt <= MMI_QUEEN; pt++)
        score += mmi_piece_value[pt] *
                 (mmi_popcount(mmi_pieces(pos, MMI_WHITE, pt)) - mmi_popcount(mmi_pieces(pos, MMI_BLACK, pt)));
    return pos->side == MMI_WHITE ? score : -score;
}
