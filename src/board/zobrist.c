#include "board/zobrist.h"

#include "util/misc.h"

MmiKey mmi_zobrist_piece[MMI_PIECE_NB][64];
MmiKey mmi_zobrist_castling[16];
MmiKey mmi_zobrist_en_passant[8];
MmiKey mmi_zobrist_side;

void mmi_zobrist_init(void) {
    MmiRng rng = {0x6D6D695F656E67ULL};
    for (int p = 0; p < MMI_PIECE_NB; p++)
        for (int sq = 0; sq < 64; sq++) mmi_zobrist_piece[p][sq] = mmi_rng_next(&rng);
    for (int i = 0; i < 16; i++) mmi_zobrist_castling[i] = mmi_rng_next(&rng);
    for (int f = 0; f < 8; f++) mmi_zobrist_en_passant[f] = mmi_rng_next(&rng);
    mmi_zobrist_side = mmi_rng_next(&rng);
}
