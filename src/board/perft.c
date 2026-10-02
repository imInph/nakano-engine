#include "board/perft.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "board/movegen.h"
#include "util/misc.h"

uint64_t mmi_perft(MmiPosition *pos, int depth) {
    MmiMoveList list;
    if (depth == 0) return 1;
    mmi_generate(pos, &list, MMI_GEN_ALL);
    if (depth == 1) return (uint64_t)list.count;
    uint64_t nodes = 0;
    for (int i = 0; i < list.count; i++) {
        mmi_position_make(pos, list.moves[i]);
        nodes += mmi_perft(pos, depth - 1);
        mmi_position_unmake(pos, list.moves[i]);
    }
    return nodes;
}

uint64_t mmi_perft_divide(MmiPosition *pos, int depth) {
    MmiMoveList list;
    char buf[6];
    uint64_t total = 0;
    mmi_generate(pos, &list, MMI_GEN_ALL);
    for (int i = 0; i < list.count; i++) {
        mmi_position_make(pos, list.moves[i]);
        uint64_t nodes = depth > 1 ? mmi_perft(pos, depth - 1) : 1;
        mmi_position_unmake(pos, list.moves[i]);
        mmi_move_to_uci(list.moves[i], buf);
        printf("%s: %" PRIu64 "\n", buf, nodes);
        total += nodes;
    }
    printf("\nNodes searched: %" PRIu64 "\n", total);
    return total;
}

int mmi_perft_suite(const char *path, int max_depth) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    static MmiPosition pos;
    char line[512];
    int failures = 0, checks = 0;
    uint64_t total = 0;
    int64_t start = mmi_now_ms();

    while (fgets(line, sizeof(line), f)) {
        char *fields = strchr(line, ';');
        if (!fields) continue;
        *fields++ = '\0';
        if (!mmi_position_set_fen(&pos, line)) {
            printf("bad fen: %s\n", line);
            failures++;
            continue;
        }
        for (char *tok = strtok(fields, ";"); tok; tok = strtok(NULL, ";")) {
            int depth;
            unsigned long long expected;
            if (sscanf(tok, " D%d %llu", &depth, &expected) != 2 || depth > max_depth) continue;
            uint64_t nodes = mmi_perft(&pos, depth);
            total += nodes;
            checks++;
            if (nodes != expected) {
                printf("FAIL depth %d: %" PRIu64 " expected %llu  %s\n", depth, nodes, expected, line);
                failures++;
            }
        }
    }
    fclose(f);

    int64_t ms = mmi_now_ms() - start;
    printf("mmi perft suite: %d checks, %d failures, %" PRIu64 " nodes, %" PRId64 " ms, %" PRIu64 " nps\n", checks,
           failures, total, ms, total * 1000 / (uint64_t)(ms > 0 ? ms : 1));
    return failures;
}
