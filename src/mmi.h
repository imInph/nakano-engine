#ifndef MMI_H
#define MMI_H

#include <stdbool.h>
#include <stdint.h>

#define MMI_NAME "mmi_engine"
#define MMI_VERSION "0.0.0"
#define MMI_AUTHORS "inph, Mai, alp"
#ifndef MMI_GIT_HASH
#define MMI_GIT_HASH "unknown"
#endif

#define MMI_MAX_MOVES 256
#define MMI_MAX_PLY 128

typedef uint64_t MmiBitboard;
typedef uint64_t MmiKey;
typedef int MmiValue;

typedef enum { MMI_WHITE, MMI_BLACK } MmiColor;

typedef enum { MMI_NO_PIECE_TYPE, MMI_PAWN, MMI_KNIGHT, MMI_BISHOP, MMI_ROOK, MMI_QUEEN, MMI_KING } MmiPieceType;

/* A piece is its type with the colour in bit 3: white 1-6, black 9-14. */
typedef enum {
    MMI_NO_PIECE,
    MMI_W_PAWN = 1,
    MMI_W_KNIGHT,
    MMI_W_BISHOP,
    MMI_W_ROOK,
    MMI_W_QUEEN,
    MMI_W_KING,
    MMI_B_PAWN = 9,
    MMI_B_KNIGHT,
    MMI_B_BISHOP,
    MMI_B_ROOK,
    MMI_B_QUEEN,
    MMI_B_KING,
    MMI_PIECE_NB = 16
} MmiPiece;

/* clang-format off */
typedef enum {
    MMI_A1, MMI_B1, MMI_C1, MMI_D1, MMI_E1, MMI_F1, MMI_G1, MMI_H1,
    MMI_A2, MMI_B2, MMI_C2, MMI_D2, MMI_E2, MMI_F2, MMI_G2, MMI_H2,
    MMI_A3, MMI_B3, MMI_C3, MMI_D3, MMI_E3, MMI_F3, MMI_G3, MMI_H3,
    MMI_A4, MMI_B4, MMI_C4, MMI_D4, MMI_E4, MMI_F4, MMI_G4, MMI_H4,
    MMI_A5, MMI_B5, MMI_C5, MMI_D5, MMI_E5, MMI_F5, MMI_G5, MMI_H5,
    MMI_A6, MMI_B6, MMI_C6, MMI_D6, MMI_E6, MMI_F6, MMI_G6, MMI_H6,
    MMI_A7, MMI_B7, MMI_C7, MMI_D7, MMI_E7, MMI_F7, MMI_G7, MMI_H7,
    MMI_A8, MMI_B8, MMI_C8, MMI_D8, MMI_E8, MMI_F8, MMI_G8, MMI_H8,
    MMI_NO_SQUARE
} MmiSquare;
/* clang-format on */

enum { MMI_WHITE_OO = 1, MMI_WHITE_OOO = 2, MMI_BLACK_OO = 4, MMI_BLACK_OOO = 8, MMI_ALL_CASTLING = 15 };

/*
 * A move is 16 bits: destination (bits 0-5), origin (6-11), promotion piece
 * type minus knight (12-13) and move type (14-15). Castling is encoded as the
 * king capturing its own rook, so the same encoding works for Chess960.
 */
typedef uint16_t MmiMove;

typedef enum {
    MMI_MOVE_NORMAL = 0,
    MMI_MOVE_PROMOTION = 1 << 14,
    MMI_MOVE_EN_PASSANT = 2 << 14,
    MMI_MOVE_CASTLING = 3 << 14
} MmiMoveType;

#define MMI_MOVE_NONE ((MmiMove)0)
#define MMI_MOVE_NULL ((MmiMove)65)

#define MMI_VALUE_DRAW 0
#define MMI_VALUE_MATE 32000
#define MMI_VALUE_INFINITE 32001
#define MMI_VALUE_NONE 32002
#define MMI_VALUE_MATE_IN_MAX_PLY (MMI_VALUE_MATE - MMI_MAX_PLY)

static inline MmiPiece mmi_make_piece(MmiColor c, MmiPieceType pt) { return (MmiPiece)((c << 3) | pt); }
static inline MmiPieceType mmi_piece_type(MmiPiece p) { return (MmiPieceType)(p & 7); }
static inline MmiColor mmi_piece_color(MmiPiece p) { return (MmiColor)(p >> 3); }

static inline int mmi_file_of(int sq) { return sq & 7; }
static inline int mmi_rank_of(int sq) { return sq >> 3; }
static inline int mmi_make_square(int file, int rank) { return rank * 8 + file; }
/* Rank as seen from side c: rank 0 is c's back rank. */
static inline int mmi_relative_rank(MmiColor c, int sq) { return mmi_rank_of(sq) ^ (c * 7); }

static inline MmiMove mmi_move_make(int from, int to, MmiMoveType type) { return (MmiMove)(type | (from << 6) | to); }
static inline MmiMove mmi_move_make_promotion(int from, int to, MmiPieceType pt) {
    return (MmiMove)(MMI_MOVE_PROMOTION | ((pt - MMI_KNIGHT) << 12) | (from << 6) | to);
}
static inline int mmi_move_from(MmiMove m) { return (m >> 6) & 63; }
static inline int mmi_move_to(MmiMove m) { return m & 63; }
static inline MmiMoveType mmi_move_type(MmiMove m) { return (MmiMoveType)(m & (3 << 14)); }
static inline MmiPieceType mmi_move_promotion(MmiMove m) { return (MmiPieceType)(((m >> 12) & 3) + MMI_KNIGHT); }

#endif
