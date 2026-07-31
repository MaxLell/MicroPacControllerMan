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
 * The interface deliberately says nothing about how the choice is made. Today it is a
 * greedy step by Manhattan distance, which §10.4 specifies and which is enough for an
 * 11x9 maze; replacing it with A* is then an internal change with no call site touched.
 * That is also why the target is a plain cell rather than anything precomputed: an A*
 * implementation needs exactly the same inputs.
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
 * Considers the open neighbours of `in_from`, skipping `in_forbidden_direction`, and
 * returns the one nearest the target. Ties break in the fixed order **Up, Left, Down,
 * Right** (§10.4), so the result is deterministic and unit-testable.
 *
 * The target need not be reachable, or even inside the maze — §10.4's Pinky and Inky
 * both aim at cells that can be walls or off-grid. A target is a thing to measure
 * against, never a cell to arrive at.
 *
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_from: the cell being left
 * \param[in]       in_target: the cell to head towards
 * \param[in]       in_forbidden_direction: the way back, normally the reverse of the
 *                      current facing so a ghost does not turn around (§10.1). Pass
 *                      \ref DIRECTION_NONE to allow any direction.
 * \return          The direction to step, or \ref DIRECTION_NONE if the cell is walled
 *                      in entirely. If the forbidden direction is the *only* way out —
 *                      a dead end — it is returned, because standing still forever is
 *                      worse than turning around.
 */
direction_e ghost_path_find_step_towards(const playfield_t* in_playfield, cell_t in_from, cell_t in_target,
                                         direction_e in_forbidden_direction);

/*! \brief Choose the step that gets furthest from a cell — the frightened behaviour.
 *
 * The mirror of #ghost_path_find_step_towards: same candidates, same tie-break order,
 * but it takes the neighbour *furthest* from the cell to avoid (§10.5).
 *
 * Deliberately a separate function rather than a flag, because fleeing has no
 * destination to plan a route to. When the seek side becomes A*, this one stays greedy.
 *
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_from: the cell being left
 * \param[in]       in_avoid: the cell to get away from — Pacman
 * \param[in]       in_forbidden_direction: as above
 * \return          The direction to step, with the same dead-end rule
 */
direction_e ghost_path_find_step_away_from(const playfield_t* in_playfield, cell_t in_from, cell_t in_avoid,
                                           direction_e in_forbidden_direction);

#endif /* GHOST_PATH_H */
