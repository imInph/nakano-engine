#ifndef MMI_SEE_H
#define MMI_SEE_H

#include "board/position.h"

/*
 * Static exchange evaluation: true if playing m and then trading off on its
 * destination, each side always recapturing with its least valuable piece and
 * free to stop, gains at least threshold centipawns for the side to move.
 * Pins are ignored. m must be legal.
 */
bool mmi_see_ge(const MmiPosition *pos, MmiMove m, int threshold);

#endif
