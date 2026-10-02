#include "uci/bench.h"

#include <inttypes.h>
#include <stdio.h>

#include "board/position.h"
#include "search/search.h"
#include "search/tt.h"
#include "util/misc.h"

static const char *const bench_fens[] = {
    MMI_START_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbqkb1r/pp1p1ppp/4pn2/2p5/2PP4/2N5/PP2PPPP/R1BQKBNR w KQkq - 0 4",
    "r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2N1PN2/PP1B1PPP/R2QKB1R w KQ - 0 8",
    "2r3k1/pp3ppp/4p3/3p4/3P4/2P1P3/P4PPP/2R3K1 w - - 0 25",
    "8/8/4k3/3p4/3P4/4K3/8/8 w - - 0 50",
    "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 40",
    "r1b2rk1/2q1bppp/p2p1n2/np2p3/3PP3/5N1P/PPBN1PP1/R1BQR1K1 w - - 0 13",
};

uint64_t mmi_bench(int depth) {
    static MmiPosition pos;
    uint64_t nodes = 0;
    int64_t start = mmi_now_ms();
    for (size_t i = 0; i < sizeof(bench_fens) / sizeof(bench_fens[0]); i++) {
        mmi_position_set_fen(&pos, bench_fens[i]);
        mmi_tt_clear();
        nodes += mmi_search_fixed_depth(&pos, depth);
    }
    int64_t ms = mmi_now_ms() - start;
    printf("mmi bench: %" PRIu64 " nodes %" PRId64 " ms %" PRIu64 " nps\n", nodes, ms,
           nodes * 1000 / (uint64_t)(ms > 0 ? ms : 1));
    fflush(stdout);
    return nodes;
}
