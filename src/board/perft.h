#ifndef MMI_PERFT_H
#define MMI_PERFT_H

#include "board/position.h"

uint64_t mmi_perft(MmiPosition *pos, int depth);
/* Prints the node count below each root move, then the total. */
uint64_t mmi_perft_divide(MmiPosition *pos, int depth);
/*
 * Runs an EPD file of lines "FEN ;D1 n ;D2 n ..." up to max_depth.
 * Returns the number of mismatches, or -1 if the file cannot be read.
 */
int mmi_perft_suite(const char *path, int max_depth);

#endif
