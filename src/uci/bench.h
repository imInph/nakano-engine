#ifndef MMI_BENCH_H
#define MMI_BENCH_H

#include <stdint.h>

#define MMI_BENCH_DEPTH 13

/* Searches a fixed set of positions to depth, prints the total and returns the node count (the bench signature). */
uint64_t mmi_bench(int depth);

#endif
