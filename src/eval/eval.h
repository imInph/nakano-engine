#ifndef MMI_EVAL_H
#define MMI_EVAL_H

#include "board/position.h"

/* Piece values in centipawns, indexed by MmiPieceType. */
extern const MmiValue mmi_piece_value[7];

/* Static evaluation in centipawns from the side to move's point of view. */
MmiValue mmi_evaluate(const MmiPosition *pos);

#endif
