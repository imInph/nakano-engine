#include "board/attacks.h"

#include "util/misc.h"

MmiBitboard mmi_pawn_attacks[2][64];
MmiBitboard mmi_knight_attacks[64];
MmiBitboard mmi_king_attacks[64];
MmiBitboard mmi_between_bb[64][64];
MmiBitboard mmi_line_bb[64][64];
MmiMagic mmi_rook_magics[64];
MmiMagic mmi_bishop_magics[64];

static MmiBitboard rook_table[0x19000];
static MmiBitboard bishop_table[0x1480];

static const int rook_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
static const int bishop_dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

/* Attack set of a piece moving by the given (file, rank) steps; sliders stop at the first blocker. */
static MmiBitboard step_attacks(int sq, const int (*dirs)[2], int ndirs, bool slide, MmiBitboard occ) {
    MmiBitboard attacks = 0;
    for (int d = 0; d < ndirs; d++) {
        int f = mmi_file_of(sq), r = mmi_rank_of(sq);
        for (;;) {
            f += dirs[d][0];
            r += dirs[d][1];
            if (f < 0 || f > 7 || r < 0 || r > 7) break;
            MmiBitboard b = mmi_square_bb(mmi_make_square(f, r));
            attacks |= b;
            if (!slide || (occ & b)) break;
        }
    }
    return attacks;
}

static void init_magics(MmiMagic *magics, MmiBitboard *table, const int (*dirs)[2], uint64_t seed) {
    static MmiBitboard occupancy[4096], reference[4096];
    static int epoch[4096], attempt;
    MmiRng rng = {seed};
    MmiBitboard *next = table;

    for (int sq = 0; sq < 64; sq++) {
        MmiMagic *m = &magics[sq];
        MmiBitboard edges = ((MMI_RANK_1_BB | MMI_RANK_8_BB) & ~mmi_rank_bb(mmi_rank_of(sq))) |
                            ((MMI_FILE_A_BB | MMI_FILE_H_BB) & ~mmi_file_bb(mmi_file_of(sq)));
        m->mask = step_attacks(sq, dirs, 4, true, 0) & ~edges;
        m->shift = 64 - mmi_popcount(m->mask);
        m->attacks = next;

        int size = 0;
        MmiBitboard b = 0;
        do {
            occupancy[size] = b;
            reference[size] = step_attacks(sq, dirs, 4, true, b);
            size++;
            b = (b - m->mask) & m->mask;
        } while (b);
        next += size;

        for (int i = 0; i < size;) {
            do {
                m->magic = mmi_rng_next(&rng) & mmi_rng_next(&rng) & mmi_rng_next(&rng);
            } while (mmi_popcount((m->magic * m->mask) >> 56) < 6);

            attempt++;
            for (i = 0; i < size; i++) {
                unsigned idx = (unsigned)(((occupancy[i] & m->mask) * m->magic) >> m->shift);
                if (epoch[idx] < attempt) {
                    epoch[idx] = attempt;
                    m->attacks[idx] = reference[i];
                } else if (m->attacks[idx] != reference[i]) {
                    break;
                }
            }
        }
    }
}

void mmi_attacks_init(void) {
    static const int knight_steps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    static const int king_steps[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    static const int white_pawn_steps[2][2] = {{1, 1}, {-1, 1}};
    static const int black_pawn_steps[2][2] = {{1, -1}, {-1, -1}};

    for (int sq = 0; sq < 64; sq++) {
        mmi_knight_attacks[sq] = step_attacks(sq, knight_steps, 8, false, 0);
        mmi_king_attacks[sq] = step_attacks(sq, king_steps, 8, false, 0);
        mmi_pawn_attacks[MMI_WHITE][sq] = step_attacks(sq, white_pawn_steps, 2, false, 0);
        mmi_pawn_attacks[MMI_BLACK][sq] = step_attacks(sq, black_pawn_steps, 2, false, 0);
    }

    init_magics(mmi_rook_magics, rook_table, rook_dirs, 0x6D6D69526F6F6BULL);
    init_magics(mmi_bishop_magics, bishop_table, bishop_dirs, 0x6D6D6942697368ULL);

    for (int a = 0; a < 64; a++) {
        for (int b = 0; b < 64; b++) {
            mmi_between_bb[a][b] = 0;
            mmi_line_bb[a][b] = 0;
            if (a == b) continue;
            MmiBitboard ab = mmi_square_bb(a) | mmi_square_bb(b);
            if (mmi_rook_attacks(a, 0) & mmi_square_bb(b)) {
                mmi_line_bb[a][b] = (mmi_rook_attacks(a, 0) & mmi_rook_attacks(b, 0)) | ab;
                mmi_between_bb[a][b] = mmi_rook_attacks(a, mmi_square_bb(b)) & mmi_rook_attacks(b, mmi_square_bb(a));
            } else if (mmi_bishop_attacks(a, 0) & mmi_square_bb(b)) {
                mmi_line_bb[a][b] = (mmi_bishop_attacks(a, 0) & mmi_bishop_attacks(b, 0)) | ab;
                mmi_between_bb[a][b] =
                    mmi_bishop_attacks(a, mmi_square_bb(b)) & mmi_bishop_attacks(b, mmi_square_bb(a));
            }
        }
    }
}
