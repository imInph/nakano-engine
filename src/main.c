#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board/perft.h"
#include "search/tt.h"
#include "uci/bench.h"
#include "uci/uci.h"

static void usage(void) {
    puts("usage: mmi_engine                          UCI mode\n"
         "       mmi_engine bench [depth]            fixed-depth search signature\n"
         "       mmi_engine perft <depth> [fen]      leaf node count\n"
         "       mmi_engine divide <depth> [fen]     node count per root move\n"
         "       mmi_engine perftsuite [file] [max]  check an EPD perft file");
}

int main(int argc, char **argv) {
    mmi_board_init();
    if (!mmi_tt_resize(16)) {
        fputs("mmi: cannot allocate the transposition table\n", stderr);
        return 1;
    }

    int status = 0;
    if (argc < 2) {
        mmi_uci_loop();
    } else if (strcmp(argv[1], "bench") == 0) {
        mmi_bench(argc >= 3 ? atoi(argv[2]) : MMI_BENCH_DEPTH);
    } else if ((strcmp(argv[1], "perft") == 0 || strcmp(argv[1], "divide") == 0) && argc >= 3) {
        static MmiPosition pos;
        if (!mmi_position_set_fen(&pos, argc >= 4 ? argv[3] : MMI_START_FEN)) {
            fputs("mmi: invalid fen\n", stderr);
            status = 1;
        } else if (argv[1][0] == 'd') {
            mmi_perft_divide(&pos, atoi(argv[2]));
        } else {
            printf("%llu\n", (unsigned long long)mmi_perft(&pos, atoi(argv[2])));
        }
    } else if (strcmp(argv[1], "perftsuite") == 0) {
        const char *path = argc >= 3 ? argv[2] : "tests/perft.epd";
        int failures = mmi_perft_suite(path, argc >= 4 ? atoi(argv[3]) : 99);
        if (failures < 0) fprintf(stderr, "mmi: cannot read %s\n", path);
        status = failures == 0 ? 0 : 1;
    } else {
        usage();
        status = 1;
    }

    mmi_tt_free();
    return status;
}
