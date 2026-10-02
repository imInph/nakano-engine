#ifndef MMI_POSITION_H
#define MMI_POSITION_H

#include <stdio.h>

#include "board/attacks.h"

#define MMI_START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define MMI_MAX_GAME_PLY 2048
#define MMI_FEN_MAX 100

/* Everything make() changes that unmake() cannot recompute. One entry per ply. */
typedef struct {
    MmiKey key;
    MmiBitboard checkers;
    int castling;
    int en_passant; /* MMI_NO_SQUARE unless an en passant capture is possible */
    int rule50;
    int plies_from_null;
    MmiPiece captured;
} MmiState;

typedef struct {
    MmiPiece board[64];
    MmiBitboard by_type[7]; /* index MMI_NO_PIECE_TYPE holds all pieces */
    MmiBitboard by_color[2];
    MmiColor side;
    int game_ply;
    int state_index;
    MmiState states[MMI_MAX_GAME_PLY];
} MmiPosition;

/* Initialises attack tables and Zobrist keys. Call once at startup. */
void mmi_board_init(void);

/* Returns false and leaves pos unspecified if the FEN is malformed. */
bool mmi_position_set_fen(MmiPosition *pos, const char *fen);
void mmi_position_get_fen(const MmiPosition *pos, char fen[MMI_FEN_MAX]);
void mmi_position_print(const MmiPosition *pos, FILE *out);

/* m must be legal. Every board change goes through put/remove/move_piece in position.c. */
void mmi_position_make(MmiPosition *pos, MmiMove m);
void mmi_position_unmake(MmiPosition *pos, MmiMove m);
void mmi_position_make_null(MmiPosition *pos);
void mmi_position_unmake_null(MmiPosition *pos);

MmiBitboard mmi_position_attackers_to(const MmiPosition *pos, int sq, MmiBitboard occ);

/* Draw by the fifty-move rule or by repetition since the last irreversible move. */
bool mmi_position_is_draw(const MmiPosition *pos);

static inline const MmiState *mmi_state(const MmiPosition *pos) { return &pos->states[pos->state_index]; }
static inline MmiKey mmi_position_key(const MmiPosition *pos) { return mmi_state(pos)->key; }
static inline MmiBitboard mmi_occupied(const MmiPosition *pos) { return pos->by_type[MMI_NO_PIECE_TYPE]; }
static inline MmiBitboard mmi_pieces(const MmiPosition *pos, MmiColor c, MmiPieceType pt) {
    return pos->by_color[c] & pos->by_type[pt];
}
static inline int mmi_king_square(const MmiPosition *pos, MmiColor c) {
    return mmi_lsb(mmi_pieces(pos, c, MMI_KING));
}
static inline bool mmi_in_check(const MmiPosition *pos) { return mmi_state(pos)->checkers != 0; }
static inline MmiPiece mmi_piece_on(const MmiPosition *pos, int sq) { return pos->board[sq]; }

static inline bool mmi_is_capture(const MmiPosition *pos, MmiMove m) {
    return (pos->board[mmi_move_to(m)] != MMI_NO_PIECE && mmi_move_type(m) != MMI_MOVE_CASTLING) ||
           mmi_move_type(m) == MMI_MOVE_EN_PASSANT;
}

#endif
