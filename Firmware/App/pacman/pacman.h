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

/*! \brief The cell Pacman occupies. */
cell_t pacman_get_cell(const pacman_t* in_pacman);

/*! \brief The direction Pacman faces — what a ghost aims ahead of (§10.4). */
direction_e pacman_get_direction(const pacman_t* in_pacman);

/*! \brief The cell a number of steps ahead of Pacman, for ghost targeting. */
cell_t pacman_get_cell_ahead(const pacman_t* in_pacman, uint8_t in_step_count);

#endif /* PACMAN_H */
