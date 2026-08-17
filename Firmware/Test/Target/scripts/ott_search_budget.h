/*
 * ott_search_budget.h — how much look-ahead this board can afford in a frame.
 */

#ifndef OTT_SEARCH_BUDGET_H
#define OTT_SEARCH_BUDGET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*! \brief Measure what a step of a forward search costs, and say how deep one could go.
 *
 * A look-ahead player would roll the game forward over its own candidate moves and the ghosts'
 * deterministic answers to them. The ghosts are the expensive half: `ghost_path` searches the maze
 * breadth-first for every ghost on every cell it enters, so one cell of simulated future costs four
 * of those searches. Everything about a search — how deep, how wide, whether it is worth building at
 * all — follows from that one number, and it is a property of this board rather than of the host.
 *
 * Reports rather than judges, except for the one thing that would make the idea pointless: if a
 * single cell of look-ahead does not fit in a frame, nothing built on it will.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: why it failed, when it does
 * \param[in]       in_reason_size: size of that buffer
 * \return          `true` when at least one cell of look-ahead fits in a frame
 */
bool ott_search_budget_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_SEARCH_BUDGET_H */
