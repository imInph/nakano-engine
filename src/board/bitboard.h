#ifndef MMI_BITBOARD_H
#define MMI_BITBOARD_H

#include "mmi.h"

#define MMI_FILE_A_BB 0x0101010101010101ULL
#define MMI_FILE_H_BB (MMI_FILE_A_BB << 7)
#define MMI_RANK_1_BB 0xFFULL
#define MMI_RANK_8_BB (MMI_RANK_1_BB << 56)

static inline MmiBitboard mmi_square_bb(int sq) { return 1ULL << sq; }
static inline MmiBitboard mmi_file_bb(int file) { return MMI_FILE_A_BB << file; }
static inline MmiBitboard mmi_rank_bb(int rank) { return MMI_RANK_1_BB << (8 * rank); }

static inline int mmi_popcount(MmiBitboard b) { return __builtin_popcountll(b); }
static inline int mmi_lsb(MmiBitboard b) { return __builtin_ctzll(b); }
static inline bool mmi_more_than_one(MmiBitboard b) { return (b & (b - 1)) != 0; }

static inline int mmi_pop_lsb(MmiBitboard *b) {
    int sq = mmi_lsb(*b);
    *b &= *b - 1;
    return sq;
}

#endif
