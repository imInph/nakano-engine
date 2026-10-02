#include "board/movegen.h"

#include <string.h>

static inline void add(MmiMoveList *list, MmiMove m) { list->moves[list->count++] = m; }

static void add_promotions(MmiMoveList *list, int from, int to, MmiGenType type) {
    if (type != MMI_GEN_QUIETS) add(list, mmi_move_make_promotion(from, to, MMI_QUEEN));
    if (type != MMI_GEN_CAPTURES) {
        add(list, mmi_move_make_promotion(from, to, MMI_KNIGHT));
        add(list, mmi_move_make_promotion(from, to, MMI_ROOK));
        add(list, mmi_move_make_promotion(from, to, MMI_BISHOP));
    }
}

/* Squares attacked by c, with occupancy occ (our king removed so it cannot hide behind itself). */
static MmiBitboard attacked_by(const MmiPosition *pos, MmiColor c, MmiBitboard occ) {
    MmiBitboard attacks = 0, b;
    b = mmi_pieces(pos, c, MMI_PAWN);
    while (b) attacks |= mmi_pawn_attacks[c][mmi_pop_lsb(&b)];
    b = mmi_pieces(pos, c, MMI_KNIGHT);
    while (b) attacks |= mmi_knight_attacks[mmi_pop_lsb(&b)];
    b = mmi_pieces(pos, c, MMI_BISHOP) | mmi_pieces(pos, c, MMI_QUEEN);
    while (b) attacks |= mmi_bishop_attacks(mmi_pop_lsb(&b), occ);
    b = mmi_pieces(pos, c, MMI_ROOK) | mmi_pieces(pos, c, MMI_QUEEN);
    while (b) attacks |= mmi_rook_attacks(mmi_pop_lsb(&b), occ);
    return attacks | mmi_king_attacks[mmi_king_square(pos, c)];
}

static void generate_pawns(const MmiPosition *pos, MmiMoveList *list, MmiGenType type, MmiBitboard checkmask,
                           MmiBitboard pinned, int ksq) {
    const MmiState *st = mmi_state(pos);
    MmiColor us = pos->side, them = (MmiColor)!us;
    MmiBitboard occ = mmi_occupied(pos), theirs = pos->by_color[them];
    int up = us == MMI_WHITE ? 8 : -8;
    MmiBitboard pawns = mmi_pieces(pos, us, MMI_PAWN);

    while (pawns) {
        int from = mmi_pop_lsb(&pawns);
        MmiBitboard allowed = checkmask;
        if (pinned & mmi_square_bb(from)) allowed &= mmi_line_bb[ksq][from];
        bool promotes = mmi_relative_rank(us, from) == 6;

        int to = from + up;
        if (!(occ & mmi_square_bb(to))) {
            if (allowed & mmi_square_bb(to)) {
                if (promotes)
                    add_promotions(list, from, to, type);
                else if (type != MMI_GEN_CAPTURES)
                    add(list, mmi_move_make(from, to, MMI_MOVE_NORMAL));
            }
            int to2 = to + up;
            if (mmi_relative_rank(us, from) == 1 && type != MMI_GEN_CAPTURES && !(occ & mmi_square_bb(to2)) &&
                (allowed & mmi_square_bb(to2)))
                add(list, mmi_move_make(from, to2, MMI_MOVE_NORMAL));
        }

        MmiBitboard captures = mmi_pawn_attacks[us][from] & theirs & allowed;
        while (captures) {
            to = mmi_pop_lsb(&captures);
            if (promotes)
                add_promotions(list, from, to, type);
            else if (type != MMI_GEN_QUIETS)
                add(list, mmi_move_make(from, to, MMI_MOVE_NORMAL));
        }

        /* En passant can expose the king along the rank, so test the resulting board directly. */
        int ep = st->en_passant;
        if (ep != MMI_NO_SQUARE && type != MMI_GEN_QUIETS && (mmi_pawn_attacks[us][from] & mmi_square_bb(ep))) {
            int cap_sq = ep - up;
            MmiBitboard after = (occ ^ mmi_square_bb(from) ^ mmi_square_bb(cap_sq)) | mmi_square_bb(ep);
            if (!(mmi_position_attackers_to(pos, ksq, after) & theirs & ~mmi_square_bb(cap_sq)))
                add(list, mmi_move_make(from, ep, MMI_MOVE_EN_PASSANT));
        }
    }
}

static void generate_castling(const MmiPosition *pos, MmiMoveList *list, MmiBitboard danger) {
    const MmiState *st = mmi_state(pos);
    MmiColor us = pos->side;
    MmiBitboard occ = mmi_occupied(pos);
    int base = us == MMI_WHITE ? 0 : 56;
    int king = base + 4;
    int oo = us == MMI_WHITE ? MMI_WHITE_OO : MMI_BLACK_OO;
    int ooo = us == MMI_WHITE ? MMI_WHITE_OOO : MMI_BLACK_OOO;

    if ((st->castling & oo) && !(occ & (mmi_square_bb(base + 5) | mmi_square_bb(base + 6))) &&
        !(danger & (mmi_square_bb(base + 5) | mmi_square_bb(base + 6))))
        add(list, mmi_move_make(king, base + 7, MMI_MOVE_CASTLING));
    if ((st->castling & ooo) &&
        !(occ & (mmi_square_bb(base + 1) | mmi_square_bb(base + 2) | mmi_square_bb(base + 3))) &&
        !(danger & (mmi_square_bb(base + 2) | mmi_square_bb(base + 3))))
        add(list, mmi_move_make(king, base, MMI_MOVE_CASTLING));
}

int mmi_generate(const MmiPosition *pos, MmiMoveList *list, MmiGenType type) {
    const MmiState *st = mmi_state(pos);
    MmiColor us = pos->side, them = (MmiColor)!us;
    MmiBitboard ours = pos->by_color[us], theirs = pos->by_color[them], occ = mmi_occupied(pos);
    int ksq = mmi_king_square(pos, us);
    MmiBitboard target = type == MMI_GEN_CAPTURES ? theirs : type == MMI_GEN_QUIETS ? ~occ : ~ours;
    MmiBitboard danger = attacked_by(pos, them, occ ^ mmi_square_bb(ksq));

    list->count = 0;

    MmiBitboard b = mmi_king_attacks[ksq] & target & ~danger;
    while (b) add(list, mmi_move_make(ksq, mmi_pop_lsb(&b), MMI_MOVE_NORMAL));

    if (mmi_more_than_one(st->checkers)) return list->count;

    MmiBitboard checkmask = ~0ULL;
    if (st->checkers) {
        int checker = mmi_lsb(st->checkers);
        checkmask = mmi_between_bb[ksq][checker] | st->checkers;
    }

    MmiBitboard pinned = 0;
    MmiBitboard snipers = ((mmi_rook_attacks(ksq, 0) & (pos->by_type[MMI_ROOK] | pos->by_type[MMI_QUEEN])) |
                           (mmi_bishop_attacks(ksq, 0) & (pos->by_type[MMI_BISHOP] | pos->by_type[MMI_QUEEN]))) &
                          theirs;
    while (snipers) {
        MmiBitboard blockers = mmi_between_bb[ksq][mmi_pop_lsb(&snipers)] & occ;
        if (blockers && !mmi_more_than_one(blockers) && (blockers & ours)) pinned |= blockers;
    }

    generate_pawns(pos, list, type, checkmask, pinned, ksq);

    for (MmiPieceType pt = MMI_KNIGHT; pt <= MMI_QUEEN; pt++) {
        MmiBitboard pieces = mmi_pieces(pos, us, pt);
        while (pieces) {
            int from = mmi_pop_lsb(&pieces);
            b = mmi_piece_attacks(pt, from, occ) & target & checkmask;
            if (pinned & mmi_square_bb(from)) b &= mmi_line_bb[ksq][from];
            while (b) add(list, mmi_move_make(from, mmi_pop_lsb(&b), MMI_MOVE_NORMAL));
        }
    }

    if (!st->checkers && type != MMI_GEN_CAPTURES) generate_castling(pos, list, danger);
    return list->count;
}

void mmi_move_to_uci(MmiMove m, char buf[6]) {
    if (m == MMI_MOVE_NONE || m == MMI_MOVE_NULL) {
        strcpy(buf, "0000");
        return;
    }
    int from = mmi_move_from(m), to = mmi_move_to(m);
    if (mmi_move_type(m) == MMI_MOVE_CASTLING) to = (from & 56) + (to > from ? 6 : 2);
    buf[0] = (char)('a' + mmi_file_of(from));
    buf[1] = (char)('1' + mmi_rank_of(from));
    buf[2] = (char)('a' + mmi_file_of(to));
    buf[3] = (char)('1' + mmi_rank_of(to));
    buf[4] = mmi_move_type(m) == MMI_MOVE_PROMOTION ? "nbrq"[mmi_move_promotion(m) - MMI_KNIGHT] : '\0';
    buf[5] = '\0';
}

MmiMove mmi_move_from_uci(const MmiPosition *pos, const char *str) {
    MmiMoveList list;
    char buf[6];
    mmi_generate(pos, &list, MMI_GEN_ALL);
    for (int i = 0; i < list.count; i++) {
        mmi_move_to_uci(list.moves[i], buf);
        if (strcmp(buf, str) == 0) return list.moves[i];
    }
    return MMI_MOVE_NONE;
}
