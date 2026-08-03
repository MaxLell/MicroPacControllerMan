/*
 * ghost_path.h
 *
 * Path finding for the ghosts: given where you are and where you want to be, which
 * neighbouring cell do you step to
 * ([10 §10.4](../../../Docu/PrePlanning/10-Pacman-Game-Design.md), FR-014/015).
 *
 * **Stateless by design** ([03 §3.6](../../../Docu/PrePlanning/03-Architecture.md) calls
 * this a library, not a module with a mailbox). It holds no data at all — no ghost, no
 * mode, no memory of previous calls. Every ghost calls the same functions; each ghost
 * owns its own state and works out its own target cell, because *what* to aim at is the
 * personality (§10.4) while *how to get there* is this algorithm.
 *
 * The interface deliberately says nothing about how the choice is made, and that has paid
 * off: seeking started as a greedy step by Manhattan distance and is now a breadth-first
 * route search, with no call site touched. The target stays a plain cell because every
 * candidate algorithm needs exactly the same inputs.
 */

#ifndef GHOST_PATH_H
#define GHOST_PATH_H

#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * ghost_path - public API
 * ========================================================================= */

/*! \brief Choose the step that gets closest to a target cell.
 *
 * Searches the maze breadth-first from `in_from`, never taking `in_forbidden_direction` as
 * the first step, and returns the first step of the shortest route to the reachable cell
 * nearest the target. Ties — equal routes to equally near cells — break in the fixed order
 * **Up, Left, Down, Right** (§10.4), so the result is deterministic and unit-testable.
 *
 * Searching the route rather than comparing the four neighbours is a deliberate departure
 * from the arcade, asked for by the owner. Where the target is reachable the two agree;
 * where it is not, comparing neighbours makes a detour cost nothing, so a ghost treats
 * visibly different options as equal and turns on tie-break order alone.
 *
 * The target need not be reachable, or even inside the maze — §10.4's Pinky and Inky both
 * aim at cells that can be walls or off-grid, and a scatter target always is. A target is
 * a thing to measure against, never a cell to arrive at, which is also why no heuristic
 * could prune this search: there is usually nothing there to be guided towards.
 *
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_from: the cell being left
 * \param[in]       in_target: the cell to head towards
 * \param[in]       in_forbidden_direction: the way back, normally the reverse of the
 *                      current facing so a ghost does not turn around (§10.1). Pass
 *                      \ref DIRECTION_NONE to allow any direction.
 * \param[in]       in_may_pass_gate: whether the route may cross the ghost house gate.
 *                      True only for a ghost on its way out of the house or being revived
 *                      in it; false otherwise, which is what keeps one from wandering back
 *                      into its cage.
 * \return          The direction to step, or \ref DIRECTION_NONE if the cell is walled
 *                      in entirely. If the forbidden direction is the *only* way out —
 *                      a dead end — it is returned, because standing still forever is
 *                      worse than turning around.
 */
direction_e ghost_path_find_step_towards(const playfield_t* in_playfield, cell_t in_from, cell_t in_target,
                                         direction_e in_forbidden_direction, bool in_may_pass_gate);

/*! \brief Choose the step that gets furthest from a cell — the frightened behaviour.
 *
 * The counterpart of #ghost_path_find_step_towards, and simpler: it compares the open
 * neighbours and takes the one *furthest* from the cell to avoid (§10.5), same tie-break
 * order.
 *
 * Deliberately a separate function rather than a flag, because fleeing has no destination
 * to plan a route to — which is why the seek side gained a route search and this one did
 * not.
 *
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_from: the cell being left
 * \param[in]       in_avoid: the cell to get away from — Pacman
 * \param[in]       in_forbidden_direction: as above
 * \param[in]       in_may_pass_gate: as above
 * \return          The direction to step, with the same dead-end rule
 */
direction_e ghost_path_find_step_away_from(const playfield_t* in_playfield, cell_t in_from, cell_t in_avoid,
                                           direction_e in_forbidden_direction, bool in_may_pass_gate);

#endif /* GHOST_PATH_H */
