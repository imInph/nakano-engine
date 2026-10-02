#include "board/position.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "board/movegen.h"
#include "board/zobrist.h"

static const char piece_chars[] = " PNBRQK  pnbrqk";

/* Castling rights lost when a move starts or ends on the square. */
static int castling_mask[64];

void mmi_board_init(void) {
    mmi_attacks_init();
    mmi_zobrist_init();
    for (int sq = 0; sq < 64; sq++) castling_mask[sq] = 0;
    castling_mask[MMI_E1] = MMI_WHITE_OO | MMI_WHITE_OOO;
    castling_mask[MMI_H1] = MMI_WHITE_OO;
    castling_mask[MMI_A1] = MMI_WHITE_OOO;
    castling_mask[MMI_E8] = MMI_BLACK_OO | MMI_BLACK_OOO;
    castling_mask[MMI_H8] = MMI_BLACK_OO;
    castling_mask[MMI_A8] = MMI_BLACK_OOO;
}

/* The only three functions that change the board. Incremental evaluation hooks belong here. */
static inline void put_piece(MmiPosition *pos, MmiPiece pc, int sq) {
    MmiBitboard b = mmi_square_bb(sq);
    pos->board[sq] = pc;
    pos->by_type[mmi_piece_type(pc)] |= b;
    pos->by_type[MMI_NO_PIECE_TYPE] |= b;
    pos->by_color[mmi_piece_color(pc)] |= b;
}

static inline void remove_piece(MmiPosition *pos, int sq) {
    MmiPiece pc = pos->board[sq];
    MmiBitboard b = mmi_square_bb(sq);
    pos->board[sq] = MMI_NO_PIECE;
    pos->by_type[mmi_piece_type(pc)] ^= b;
    pos->by_type[MMI_NO_PIECE_TYPE] ^= b;
    pos->by_color[mmi_piece_color(pc)] ^= b;
}

static inline void move_piece(MmiPosition *pos, int from, int to) {
    MmiPiece pc = pos->board[from];
    MmiBitboard b = mmi_square_bb(from) | mmi_square_bb(to);
    pos->board[from] = MMI_NO_PIECE;
    pos->board[to] = pc;
    pos->by_type[mmi_piece_type(pc)] ^= b;
    pos->by_type[MMI_NO_PIECE_TYPE] ^= b;
    pos->by_color[mmi_piece_color(pc)] ^= b;
}

MmiBitboard mmi_position_attackers_to(const MmiPosition *pos, int sq, MmiBitboard occ) {
    return (mmi_pawn_attacks[MMI_BLACK][sq] & mmi_pieces(pos, MMI_WHITE, MMI_PAWN)) |
           (mmi_pawn_attacks[MMI_WHITE][sq] & mmi_pieces(pos, MMI_BLACK, MMI_PAWN)) |
           (mmi_knight_attacks[sq] & pos->by_type[MMI_KNIGHT]) | (mmi_king_attacks[sq] & pos->by_type[MMI_KING]) |
           (mmi_bishop_attacks(sq, occ) & (pos->by_type[MMI_BISHOP] | pos->by_type[MMI_QUEEN])) |
           (mmi_rook_attacks(sq, occ) & (pos->by_type[MMI_ROOK] | pos->by_type[MMI_QUEEN]));
}

/* An en passant square only counts (and is hashed) if a pawn of the side to move can capture there. */
static bool en_passant_possible(const MmiPosition *pos, int sq) {
    MmiColor us = pos->side;
    return (mmi_pawn_attacks[!us][sq] & mmi_pieces(pos, us, MMI_PAWN)) != 0;
}

static MmiKey compute_key(const MmiPosition *pos) {
    const MmiState *st = mmi_state(pos);
    MmiKey key = mmi_zobrist_castling[st->castling];
    for (int sq = 0; sq < 64; sq++)
        if (pos->board[sq] != MMI_NO_PIECE) key ^= mmi_zobrist_piece[pos->board[sq]][sq];
    if (st->en_passant != MMI_NO_SQUARE) key ^= mmi_zobrist_en_passant[mmi_file_of(st->en_passant)];
    if (pos->side == MMI_BLACK) key ^= mmi_zobrist_side;
    return key;
}

bool mmi_position_set_fen(MmiPosition *pos, const char *fen) {
    memset(pos, 0, sizeof(*pos));
    MmiState *st = &pos->states[0];
    const char *p = fen;
    while (*p == ' ') p++;

    int file = 0, rank = 7;
    for (; *p && *p != ' '; p++) {
        if (*p == '/') {
            if (file != 8 || rank == 0) return false;
            file = 0;
            rank--;
        } else if (*p >= '1' && *p <= '8') {
            file += *p - '0';
            if (file > 8) return false;
        } else {
            const char *c = strchr(piece_chars, *p);
            if (!c || *p == ' ' || file > 7) return false;
            put_piece(pos, (MmiPiece)(c - piece_chars), mmi_make_square(file, rank));
            file++;
        }
    }
    if (file != 8 || rank != 0) return false;
    if (mmi_popcount(mmi_pieces(pos, MMI_WHITE, MMI_KING)) != 1 ||
        mmi_popcount(mmi_pieces(pos, MMI_BLACK, MMI_KING)) != 1)
        return false;
    if (pos->by_type[MMI_PAWN] & (MMI_RANK_1_BB | MMI_RANK_8_BB)) return false;

    while (*p == ' ') p++;
    if (*p == 'w')
        pos->side = MMI_WHITE;
    else if (*p == 'b')
        pos->side = MMI_BLACK;
    else
        return false;
    p++;

    while (*p == ' ') p++;
    for (; *p && *p != ' '; p++) {
        switch (*p) {
        case 'K':
            st->castling |= MMI_WHITE_OO;
            break;
        case 'Q':
            st->castling |= MMI_WHITE_OOO;
            break;
        case 'k':
            st->castling |= MMI_BLACK_OO;
            break;
        case 'q':
            st->castling |= MMI_BLACK_OOO;
            break;
        case '-':
            break;
        default:
            return false;
        }
    }
    /* Drop rights whose king or rook is not on its home square. */
    if (pos->board[MMI_E1] != MMI_W_KING) st->castling &= ~(MMI_WHITE_OO | MMI_WHITE_OOO);
    if (pos->board[MMI_H1] != MMI_W_ROOK) st->castling &= ~MMI_WHITE_OO;
    if (pos->board[MMI_A1] != MMI_W_ROOK) st->castling &= ~MMI_WHITE_OOO;
    if (pos->board[MMI_E8] != MMI_B_KING) st->castling &= ~(MMI_BLACK_OO | MMI_BLACK_OOO);
    if (pos->board[MMI_H8] != MMI_B_ROOK) st->castling &= ~MMI_BLACK_OO;
    if (pos->board[MMI_A8] != MMI_B_ROOK) st->castling &= ~MMI_BLACK_OOO;

    while (*p == ' ') p++;
    st->en_passant = MMI_NO_SQUARE;
    if (*p >= 'a' && *p <= 'h' && (p[1] == '3' || p[1] == '6')) {
        int sq = mmi_make_square(*p - 'a', p[1] - '1');
        if (mmi_relative_rank(pos->side, sq) == 5 && en_passant_possible(pos, sq)) st->en_passant = sq;
        p += 2;
    } else if (*p == '-') {
        p++;
    } else if (*p) {
        return false;
    }

    int rule50 = 0, fullmove = 1;
    if (sscanf(p, "%d %d", &rule50, &fullmove) < 1) rule50 = 0;
    st->rule50 = rule50 < 0 ? 0 : rule50;
    pos->game_ply = 2 * (fullmove > 1 ? fullmove - 1 : 0) + (pos->side == MMI_BLACK);

    /* The side not to move must not be in check. */
    MmiColor them = (MmiColor)!pos->side;
    if (mmi_position_attackers_to(pos, mmi_king_square(pos, them), mmi_occupied(pos)) & pos->by_color[pos->side])
        return false;

    st->checkers = mmi_position_attackers_to(pos, mmi_king_square(pos, pos->side), mmi_occupied(pos)) &
                   pos->by_color[them];
    st->captured = MMI_NO_PIECE;
    st->key = compute_key(pos);
    return true;
}

void mmi_position_get_fen(const MmiPosition *pos, char fen[MMI_FEN_MAX]) {
    const MmiState *st = mmi_state(pos);
    char *p = fen;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            MmiPiece pc = pos->board[mmi_make_square(file, rank)];
            if (pc == MMI_NO_PIECE) {
                empty++;
                continue;
            }
            if (empty) *p++ = (char)('0' + empty);
            empty = 0;
            *p++ = piece_chars[pc];
        }
        if (empty) *p++ = (char)('0' + empty);
        if (rank) *p++ = '/';
    }
    *p++ = ' ';
    *p++ = pos->side == MMI_WHITE ? 'w' : 'b';
    *p++ = ' ';
    if (!st->castling) *p++ = '-';
    if (st->castling & MMI_WHITE_OO) *p++ = 'K';
    if (st->castling & MMI_WHITE_OOO) *p++ = 'Q';
    if (st->castling & MMI_BLACK_OO) *p++ = 'k';
    if (st->castling & MMI_BLACK_OOO) *p++ = 'q';
    *p++ = ' ';
    if (st->en_passant == MMI_NO_SQUARE) {
        *p++ = '-';
    } else {
        *p++ = (char)('a' + mmi_file_of(st->en_passant));
        *p++ = (char)('1' + mmi_rank_of(st->en_passant));
    }
    snprintf(p, (size_t)(MMI_FEN_MAX - (p - fen)), " %d %d", st->rule50, pos->game_ply / 2 + 1);
}

void mmi_position_print(const MmiPosition *pos, FILE *out) {
    char fen[MMI_FEN_MAX];
    fprintf(out, "\n +---+---+---+---+---+---+---+---+\n");
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) fprintf(out, " | %c", piece_chars[pos->board[mmi_make_square(file, rank)]]);
        fprintf(out, " | %d\n +---+---+---+---+---+---+---+---+\n", rank + 1);
    }
    mmi_position_get_fen(pos, fen);
    fprintf(out, "   a   b   c   d   e   f   g   h\n\nFen: %s\nKey: %016" PRIX64 "\n", fen, mmi_position_key(pos));
}

void mmi_position_make(MmiPosition *pos, MmiMove m) {
    const MmiState *prev = mmi_state(pos);
    MmiState *st = &pos->states[++pos->state_index];
    MmiColor us = pos->side, them = (MmiColor)!us;
    int from = mmi_move_from(m), to = mmi_move_to(m);
    MmiMoveType type = mmi_move_type(m);
    MmiPiece pc = pos->board[from];
    MmiKey key = prev->key ^ mmi_zobrist_side;

    st->castling = prev->castling;
    st->rule50 = prev->rule50 + 1;
    st->plies_from_null = prev->plies_from_null + 1;
    st->en_passant = MMI_NO_SQUARE;
    st->captured = MMI_NO_PIECE;
    if (prev->en_passant != MMI_NO_SQUARE) key ^= mmi_zobrist_en_passant[mmi_file_of(prev->en_passant)];

    if (type == MMI_MOVE_CASTLING) {
        bool kingside = to > from;
        int rank_base = from & 56;
        int king_to = rank_base + (kingside ? 6 : 2), rook_to = rank_base + (kingside ? 5 : 3);
        MmiPiece rook = pos->board[to];
        remove_piece(pos, from);
        remove_piece(pos, to);
        put_piece(pos, pc, king_to);
        put_piece(pos, rook, rook_to);
        key ^= mmi_zobrist_piece[pc][from] ^ mmi_zobrist_piece[pc][king_to] ^ mmi_zobrist_piece[rook][to] ^
               mmi_zobrist_piece[rook][rook_to];
    } else {
        int cap_sq = type == MMI_MOVE_EN_PASSANT ? to + (us == MMI_WHITE ? -8 : 8) : to;
        MmiPiece captured = pos->board[cap_sq];
        if (captured != MMI_NO_PIECE) {
            remove_piece(pos, cap_sq);
            key ^= mmi_zobrist_piece[captured][cap_sq];
            st->captured = captured;
            st->rule50 = 0;
        }
        move_piece(pos, from, to);
        key ^= mmi_zobrist_piece[pc][from] ^ mmi_zobrist_piece[pc][to];

        if (mmi_piece_type(pc) == MMI_PAWN) {
            st->rule50 = 0;
            if ((from ^ to) == 16) {
                int ep = (from + to) / 2;
                if (mmi_pawn_attacks[us][ep] & mmi_pieces(pos, them, MMI_PAWN)) {
                    st->en_passant = ep;
                    key ^= mmi_zobrist_en_passant[mmi_file_of(ep)];
                }
            } else if (type == MMI_MOVE_PROMOTION) {
                MmiPiece promo = mmi_make_piece(us, mmi_move_promotion(m));
                remove_piece(pos, to);
                put_piece(pos, promo, to);
                key ^= mmi_zobrist_piece[pc][to] ^ mmi_zobrist_piece[promo][to];
            }
        }
    }

    int lost = castling_mask[from] | castling_mask[to];
    if (st->castling & lost) {
        key ^= mmi_zobrist_castling[st->castling];
        st->castling &= ~lost;
        key ^= mmi_zobrist_castling[st->castling];
    }

    pos->side = them;
    pos->game_ply++;
    st->key = key;
    st->checkers = mmi_position_attackers_to(pos, mmi_king_square(pos, them), mmi_occupied(pos)) & pos->by_color[us];
    assert(st->key == compute_key(pos));
}

void mmi_position_unmake(MmiPosition *pos, MmiMove m) {
    const MmiState *st = mmi_state(pos);
    MmiColor them = pos->side, us = (MmiColor)!them;
    int from = mmi_move_from(m), to = mmi_move_to(m);
    MmiMoveType type = mmi_move_type(m);

    if (type == MMI_MOVE_CASTLING) {
        bool kingside = to > from;
        int rank_base = from & 56;
        int king_to = rank_base + (kingside ? 6 : 2), rook_to = rank_base + (kingside ? 5 : 3);
        MmiPiece king = pos->board[king_to], rook = pos->board[rook_to];
        remove_piece(pos, king_to);
        remove_piece(pos, rook_to);
        put_piece(pos, king, from);
        put_piece(pos, rook, to);
    } else {
        if (type == MMI_MOVE_PROMOTION) {
            remove_piece(pos, to);
            put_piece(pos, mmi_make_piece(us, MMI_PAWN), to);
        }
        move_piece(pos, to, from);
        if (st->captured != MMI_NO_PIECE) {
            int cap_sq = type == MMI_MOVE_EN_PASSANT ? to + (us == MMI_WHITE ? -8 : 8) : to;
            put_piece(pos, st->captured, cap_sq);
        }
    }

    pos->side = us;
    pos->game_ply--;
    pos->state_index--;
}

void mmi_position_make_null(MmiPosition *pos) {
    const MmiState *prev = mmi_state(pos);
    MmiState *st = &pos->states[++pos->state_index];
    *st = *prev;
    st->key ^= mmi_zobrist_side;
    if (prev->en_passant != MMI_NO_SQUARE) st->key ^= mmi_zobrist_en_passant[mmi_file_of(prev->en_passant)];
    st->en_passant = MMI_NO_SQUARE;
    st->rule50++;
    st->plies_from_null = 0;
    st->captured = MMI_NO_PIECE;
    st->checkers = 0;
    pos->side = (MmiColor)!pos->side;
    pos->game_ply++;
    assert(st->key == compute_key(pos));
}

void mmi_position_unmake_null(MmiPosition *pos) {
    pos->side = (MmiColor)!pos->side;
    pos->game_ply--;
    pos->state_index--;
}

bool mmi_position_is_draw(const MmiPosition *pos) {
    const MmiState *st = mmi_state(pos);
    if (st->rule50 >= 100) {
        if (!st->checkers) return true;
        MmiMoveList list;
        return mmi_generate(pos, &list, MMI_GEN_ALL) > 0;
    }
    int end = st->rule50 < st->plies_from_null ? st->rule50 : st->plies_from_null;
    if (end > pos->state_index) end = pos->state_index;
    for (int i = 4; i <= end; i += 2)
        if (pos->states[pos->state_index - i].key == st->key) return true;
    return false;
}
