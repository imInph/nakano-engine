#ifndef MMI_MOVEGEN_H
#define MMI_MOVEGEN_H

#include "board/position.h"

typedef struct {
    MmiMove moves[MMI_MAX_MOVES];
    int count;
} MmiMoveList;

/*
 * All generated moves are legal. CAPTURES and QUIETS are disjoint and together
 * give ALL: CAPTURES holds captures, en passant and queen promotions (also
 * non-capturing ones), QUIETS holds the rest, including under-promotions.
 */
typedef enum { MMI_GEN_CAPTURES, MMI_GEN_QUIETS, MMI_GEN_ALL } MmiGenType;

int mmi_generate(const MmiPosition *pos, MmiMoveList *list, MmiGenType type);

/* Writes UCI notation ("e2e4", "e7e8q", castling as king to its destination). buf needs 6 bytes. */
void mmi_move_to_uci(MmiMove m, char buf[6]);
/* Returns the legal move matching the UCI string, or MMI_MOVE_NONE. */
MmiMove mmi_move_from_uci(const MmiPosition *pos, const char *str);

#endif
