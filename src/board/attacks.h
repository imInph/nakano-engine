#ifndef MMI_ATTACKS_H
#define MMI_ATTACKS_H

#include "board/bitboard.h"

typedef struct {
    MmiBitboard mask;
    MmiBitboard magic;
    MmiBitboard *attacks;
    int shift;
} MmiMagic;

extern MmiBitboard mmi_pawn_attacks[2][64];
extern MmiBitboard mmi_knight_attacks[64];
extern MmiBitboard mmi_king_attacks[64];
/* Squares strictly between two aligned squares, otherwise empty. */
extern MmiBitboard mmi_between_bb[64][64];
/* The full line through two aligned squares, otherwise empty. */
extern MmiBitboard mmi_line_bb[64][64];
extern MmiMagic mmi_rook_magics[64];
extern MmiMagic mmi_bishop_magics[64];

/* Fills all tables. Magics are searched with a fixed seed, so this is deterministic. */
void mmi_attacks_init(void);

static inline MmiBitboard mmi_magic_attacks(const MmiMagic *m, MmiBitboard occ) {
    return m->attacks[((occ & m->mask) * m->magic) >> m->shift];
}

static inline MmiBitboard mmi_bishop_attacks(int sq, MmiBitboard occ) {
    return mmi_magic_attacks(&mmi_bishop_magics[sq], occ);
}

static inline MmiBitboard mmi_rook_attacks(int sq, MmiBitboard occ) {
    return mmi_magic_attacks(&mmi_rook_magics[sq], occ);
}

static inline MmiBitboard mmi_queen_attacks(int sq, MmiBitboard occ) {
    return mmi_bishop_attacks(sq, occ) | mmi_rook_attacks(sq, occ);
}

static inline MmiBitboard mmi_piece_attacks(MmiPieceType pt, int sq, MmiBitboard occ) {
    switch (pt) {
    case MMI_KNIGHT:
        return mmi_knight_attacks[sq];
    case MMI_BISHOP:
        return mmi_bishop_attacks(sq, occ);
    case MMI_ROOK:
        return mmi_rook_attacks(sq, occ);
    case MMI_QUEEN:
        return mmi_queen_attacks(sq, occ);
    case MMI_KING:
        return mmi_king_attacks[sq];
    default:
        return 0;
    }
}

#endif
