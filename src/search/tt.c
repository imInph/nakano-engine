#include "search/tt.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Each entry is two words: the data and the key XORed with the data. A reader
 * that sees a half-written entry gets a key that does not match, so threads can
 * share the table without locks (Hyatt and Mann's lockless hashing). Relaxed
 * atomics make the concurrent access defined behaviour without costing anything
 * on x86-64 or arm64.
 */
typedef struct {
    _Atomic uint64_t key_xor_data;
    _Atomic uint64_t data;
} Entry;

#define CLUSTER_SIZE 4

typedef struct {
    Entry entries[CLUSTER_SIZE];
} Cluster;

/* data layout: move 0-15, value 16-31, depth + DEPTH_OFFSET 32-39, bound 40-41, generation 42-46. */
#define DEPTH_OFFSET 8
#define GENERATION_MASK 31

static void *allocation;
static Cluster *clusters;
static size_t cluster_count;
static unsigned generation;

static uint64_t pack(MmiValue value, MmiMove move, int depth, MmiBound bound) {
    return (uint64_t)move | ((uint64_t)(uint16_t)(int16_t)value << 16) | ((uint64_t)(depth + DEPTH_OFFSET) << 32) |
           ((uint64_t)bound << 40) | ((uint64_t)generation << 42);
}

static MmiMove data_move(uint64_t d) { return (MmiMove)(d & 0xFFFF); }
static int data_depth(uint64_t d) { return (int)((d >> 32) & 0xFF) - DEPTH_OFFSET; }
static MmiBound data_bound(uint64_t d) { return (MmiBound)((d >> 40) & 3); }
static unsigned data_generation(uint64_t d) { return (unsigned)(d >> 42) & GENERATION_MASK; }

/* How many searches ago the entry was written; older entries are cheaper to replace. */
static int data_age(uint64_t d) { return (int)((generation - data_generation(d)) & GENERATION_MASK); }

/* High 64 bits of key * cluster_count: maps the key onto any table size without a modulo. */
static Cluster *cluster_of(MmiKey key) {
    uint64_t n = cluster_count, k_lo = key & 0xFFFFFFFF, k_hi = key >> 32, n_lo = n & 0xFFFFFFFF, n_hi = n >> 32;
    uint64_t mid = ((k_lo * n_lo) >> 32) + ((k_hi * n_lo) & 0xFFFFFFFF) + k_lo * n_hi;
    return &clusters[k_hi * n_hi + ((k_hi * n_lo) >> 32) + (mid >> 32)];
}

bool mmi_tt_resize(size_t mb) {
    size_t count = mb * 1024 * 1024 / sizeof(Cluster);
    if (count == 0) count = 1;
    /* malloc only guarantees 16-byte alignment, so align clusters to cache lines by hand. */
    void *fresh = malloc(count * sizeof(Cluster) + 63);
    if (!fresh) return false;
    free(allocation);
    allocation = fresh;
    clusters = (Cluster *)(((uintptr_t)fresh + 63) & ~(uintptr_t)63);
    cluster_count = count;
    mmi_tt_clear();
    return true;
}

void mmi_tt_clear(void) {
    for (size_t i = 0; i < cluster_count; i++)
        for (int j = 0; j < CLUSTER_SIZE; j++) {
            atomic_store_explicit(&clusters[i].entries[j].key_xor_data, 0, memory_order_relaxed);
            atomic_store_explicit(&clusters[i].entries[j].data, 0, memory_order_relaxed);
        }
    generation = 0;
}

void mmi_tt_free(void) {
    free(allocation);
    allocation = NULL;
    clusters = NULL;
    cluster_count = 0;
}

void mmi_tt_new_search(void) { generation = (generation + 1) & GENERATION_MASK; }

bool mmi_tt_probe(MmiKey key, MmiTTData *out) {
    Cluster *c = cluster_of(key);
    for (int i = 0; i < CLUSTER_SIZE; i++) {
        uint64_t d = atomic_load_explicit(&c->entries[i].data, memory_order_relaxed);
        uint64_t k = atomic_load_explicit(&c->entries[i].key_xor_data, memory_order_relaxed) ^ d;
        if (d != 0 && k == key) {
            out->value = (int16_t)(uint16_t)(d >> 16);
            out->move = data_move(d);
            out->depth = data_depth(d);
            out->bound = data_bound(d);
            return true;
        }
    }
    return false;
}

void mmi_tt_store(MmiKey key, MmiValue value, MmiMove move, int depth, MmiBound bound) {
    Cluster *c = cluster_of(key);
    Entry *replace = NULL;
    int worst = 0;
    for (int i = 0; i < CLUSTER_SIZE; i++) {
        Entry *e = &c->entries[i];
        uint64_t d = atomic_load_explicit(&e->data, memory_order_relaxed);
        uint64_t k = atomic_load_explicit(&e->key_xor_data, memory_order_relaxed) ^ d;
        if (d == 0) {
            replace = e;
            break;
        }
        if (k == key) {
            /* Keep a deeper result for the same position unless the new one is exact. */
            if (data_depth(d) > depth && bound != MMI_BOUND_EXACT) return;
            if (move == MMI_MOVE_NONE) move = data_move(d);
            replace = e;
            break;
        }
        int score = data_depth(d) - 8 * data_age(d);
        if (!replace || score < worst) {
            replace = e;
            worst = score;
        }
    }
    uint64_t d = pack(value, move, depth, bound);
    atomic_store_explicit(&replace->key_xor_data, key ^ d, memory_order_relaxed);
    atomic_store_explicit(&replace->data, d, memory_order_relaxed);
}

int mmi_tt_hashfull(void) {
    size_t n = cluster_count < 1000 ? cluster_count : 1000, used = 0;
    for (size_t i = 0; i < n; i++)
        for (int j = 0; j < CLUSTER_SIZE; j++) {
            uint64_t d = atomic_load_explicit(&clusters[i].entries[j].data, memory_order_relaxed);
            used += d != 0 && data_generation(d) == generation;
        }
    return (int)(used * 1000 / (n * CLUSTER_SIZE));
}
