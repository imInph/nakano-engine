#ifndef MMI_ZOBRIST_H
#define MMI_ZOBRIST_H

#include "mmi.h"

extern MmiKey mmi_zobrist_piece[MMI_PIECE_NB][64];
extern MmiKey mmi_zobrist_castling[16];
extern MmiKey mmi_zobrist_en_passant[8];
extern MmiKey mmi_zobrist_side;

/* Fills the keys from a fixed seed, so hashes are identical across runs and machines. */
void mmi_zobrist_init(void);

#endif
