/*
 * agent.h
 *
 * The shared base for everything that moves through the maze
 * ([03 §3.6](../../../Docu/PrePlanning/03-Architecture.md), [10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)):
 * a cell, a facing direction, and a step that respects walls (FR-010) and tunnels
 * (FR-012).
 *
 * Pacman and each ghost *are* agents and specialise only **how they choose** their next
 * direction — Pacman from the player's input, a ghost from path planning. Keeping the
 * stepping here means the wall and tunnel rules exist once instead of twice.
 */

#ifndef AGENT_H
#define AGENT_H

#include <stdbool.h>

#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * agent - public types
 * ========================================================================= */

typedef struct
{
    cell_t cell;
    direction_e direction; /*!< Which way it is facing            */
} agent_t;

/* ==========================================================================
 * agent - public API
 * ========================================================================= */

/*! \brief Place an agent and set its facing.
 *
 * \param[out]      inout_agent: agent to place, must not be `NULL`
 * \param[in]       in_cell: cell to occupy
 * \param[in]       in_direction: initial facing, may be \ref DIRECTION_NONE
 */
void agent_place(agent_t* inout_agent, cell_t in_cell, direction_e in_direction);

/*! \brief Whether the neighbouring cell in a direction can be entered.
 *
 * The tunnel wrap is applied first, so a step off an edge is judged by what is on the
 * far side.
 *
 * \param[in]       in_agent: placed agent, must not be `NULL`
 * \param[in]       in_playfield: the maze, must not be `NULL`
 * \param[in]       in_direction: direction to test; \ref DIRECTION_NONE is never a step
 * \return          `true` when a step that way would land on an open cell
 */
bool agent_can_step(const agent_t* in_agent, const playfield_t* in_playfield, direction_e in_direction);

/*! \brief Face a direction and step one cell that way if it is open.
 *
 * The facing is updated even when the step is blocked, which is what lets an entity sit
 * against a wall pointing at it (§10.1).
 *
 * \param[in,out]   inout_agent: placed agent
 * \param[in]       in_playfield: the maze
 * \param[in]       in_direction: direction to move
 * \return          `true` when the agent actually moved
 */
bool agent_step(agent_t* inout_agent, const playfield_t* in_playfield, direction_e in_direction);

/*! \brief The cell the agent occupies. */
cell_t agent_get_cell(const agent_t* in_agent);

/*! \brief The direction the agent faces. */
direction_e agent_get_direction(const agent_t* in_agent);

/*! \brief The cell a number of steps ahead of the agent, for a ghost aiming at where
 *         something is going rather than where it is (§10.4).
 *
 * The result is wrapped into the grid but may be a wall — a target cell is only ever a
 * thing to measure distance to, never a cell to enter.
 *
 * \param[in]       in_agent: placed agent
 * \param[in]       in_step_count: how far ahead to look
 * \return          The cell that far ahead along the current facing
 */
cell_t agent_get_cell_ahead(const agent_t* in_agent, uint8_t in_step_count);

#endif /* AGENT_H */
