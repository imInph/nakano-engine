#include "search/tt.h"

#include <stdlib.h>
#include <string.h>

static MmiTTEntry *table;
static size_t entry_count;

bool mmi_tt_resize(size_t mb) {
    size_t count = 1;
    while (count * 2 * sizeof(MmiTTEntry) <= mb * 1024 * 1024) count *= 2;
    MmiTTEntry *fresh = calloc(count, sizeof(MmiTTEntry));
    if (!fresh) return false;
    free(table);
    table = fresh;
    entry_count = count;
    return true;
}

void mmi_tt_clear(void) { memset(table, 0, entry_count * sizeof(MmiTTEntry)); }

void mmi_tt_free(void) {
    free(table);
    table = NULL;
    entry_count = 0;
}

const MmiTTEntry *mmi_tt_probe(MmiKey key) {
    const MmiTTEntry *e = &table[key & (entry_count - 1)];
    return e->key == key && e->bound != MMI_BOUND_NONE ? e : NULL;
}

void mmi_tt_store(MmiKey key, MmiValue value, MmiMove move, int depth, MmiBound bound) {
    MmiTTEntry *e = &table[key & (entry_count - 1)];
    /* Keep a deeper result for the same position unless the new one is exact. */
    if (e->key == key && e->depth > depth && bound != MMI_BOUND_EXACT) return;
    if (move == MMI_MOVE_NONE && e->key == key) move = e->move;
    e->key = key;
    e->value = (int16_t)value;
    e->move = move;
    e->depth = (int8_t)depth;
    e->bound = (uint8_t)bound;
}

int mmi_tt_hashfull(void) {
    size_t n = entry_count < 1000 ? entry_count : 1000, used = 0;
    for (size_t i = 0; i < n; i++) used += table[i].bound != MMI_BOUND_NONE;
    return (int)(used * 1000 / n);
}
