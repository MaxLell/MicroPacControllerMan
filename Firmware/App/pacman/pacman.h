/*
 * pacman.h
 *
 * The player character: an #agent_t that follows the player's intent
 * ([10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md), FR-004/010/012).
 *
 * The *queued* direction is what makes the controls feel right. Input arrives whenever
 * the player presses, but Pacman only turns when he is due to move — so a press made a
 * little early still takes effect at the next junction instead of being dropped. Held
 * against a wall he keeps the intent and turns as soon as the way opens.
 */

#ifndef PACMAN_H
#define PACMAN_H

#include <stdbool.h>

#include "agent.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * pacman - public types
 * ========================================================================= */

typedef struct
{
    agent_t agent;
    direction_e queued_direction; /*!< What the player last asked for    */
} pacman_t;

/* ==========================================================================
 * pacman - public API
 * ========================================================================= */

/*! \brief Put Pacman at a level's start cell, facing nowhere and with no intent.
 *
 * \param[out]      inout_pacman: instance to reset, must not be `NULL`
 * \param[in]       in_start_cell: from #playfield_get_pacman_start
 */
void pacman_reset(pacman_t* inout_pacman, cell_t in_start_cell);

/*! \brief Record the player's intent (FR-004).
 *
 * Takes effect at the next move, not immediately. \ref DIRECTION_NONE is ignored so an
 * empty input cannot cancel a pending turn.
 *
 * \param[in,out]   inout_pacman: instance
 * \param[in]       in_direction: the direction the player asked for
 */
void pacman_set_intent(pacman_t* inout_pacman, direction_e in_direction);

/*! \brief Advance Pacman one cell, per §10.1.
 *
 * Adopts the queued direction if it is not blocked, then moves one cell if that cell is
 * open — otherwise he stays put, still facing that way.
 *
 * \param[in,out]   inout_pacman: instance
 * \param[in]       in_playfield: the maze
 * \return          `true` when he actually moved
 */
bool pacman_advance(pacman_t* inout_pacman, const playfield_t* in_playfield);

/*! \brief Whether a step that way is one Pacman is allowed to take.
 *
 * Walls **and** the ghost house: §10.2 puts the house off-limits to him altogether, and the gate
 * is the only way across its boundary, so barring him at the gate bars him from all of it without
 * the house having to be a wall to the ghosts who live in it.
 *
 * Public because a caller that has to enumerate his options — a look-ahead search asking which
 * ways out of this cell are worth simulating — must ask the same question #pacman_advance asks
 * itself. Two copies of this rule would be two answers the day one of them is edited.
 *
 * \param[in]       in_pacman: instance, must not be `NULL`
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_direction: direction to test; \ref DIRECTION_NONE is never a step
 * \return          `true` when he may step that way
 */
bool pacman_may_step(const pacman_t* in_pacman, const playfield_t* in_playfield, direction_e in_direction);

/*! \brief The cell Pacman occupies. */
cell_t pacman_get_cell(const pacman_t* in_pacman);

/*! \brief The direction Pacman faces — what a ghost aims ahead of (§10.4). */
direction_e pacman_get_direction(const pacman_t* in_pacman);

/*! \brief The cell a number of steps ahead of Pacman, for ghost targeting. */
cell_t pacman_get_cell_ahead(const pacman_t* in_pacman, uint8_t in_step_count);

#endif /* PACMAN_H */
