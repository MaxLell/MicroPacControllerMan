/*
 * ott_lookahead_cost.h — what a look-ahead decision costs on this board, unattended.
 */

#ifndef OTT_LOOKAHEAD_COST_H
#define OTT_LOOKAHEAD_COST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*! \brief Price a whole `pacman_lookahead` decision on the target and judge it against the frame.
 *
 * `search_budget` prices the *ghost* step a simulated cell is mostly made of, which is what
 * [DEC-049](../../../../Docu/PrePlanning/11-Decisions-and-As-Built.md) argued the greedy rule back
 * on. This prices what was built with that argument: a decision, ticks and clones and evaluation
 * included, over positions a real run walks through rather than over the opening cell.
 *
 * The two figures worth having are the **worst** decision, because that is the frame a player
 * would see stutter, and the **cost of a simulated cell**, because that is the number any future
 * change to the depth or the budget is arithmetic on.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: why it failed, when it does
 * \param[in]       in_reason_size: size of that buffer
 * \return          `true` when the worst decision still fits the frame's spare time
 */
bool ott_lookahead_cost_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_LOOKAHEAD_COST_H */
