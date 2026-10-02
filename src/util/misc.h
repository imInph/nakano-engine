#ifndef MMI_MISC_H
#define MMI_MISC_H

#include <stdint.h>

/* xorshift64*: small, fast and reproducible from a fixed seed. */
typedef struct {
    uint64_t state;
} MmiRng;

static inline uint64_t mmi_rng_next(MmiRng *rng) {
    rng->state ^= rng->state >> 12;
    rng->state ^= rng->state << 25;
    rng->state ^= rng->state >> 27;
    return rng->state * 2685821657736338717ULL;
}

/* Monotonic wall-clock time in milliseconds. */
int64_t mmi_now_ms(void);

#endif
