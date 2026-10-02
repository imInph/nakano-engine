#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board/perft.h"

int main(int argc, char **argv) {
    mmi_board_init();
    if (argc >= 2 && strcmp(argv[1], "perftsuite") == 0) {
        int failures = mmi_perft_suite(argc >= 3 ? argv[2] : "tests/perft.epd", argc >= 4 ? atoi(argv[3]) : 99);
        return failures == 0 ? 0 : 1;
    }
    return 0;
}
