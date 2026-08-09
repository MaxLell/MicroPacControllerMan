#include "pacman.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "agent.h"
#include "custom_assert.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * pacman - public
 * ========================================================================= */

void pacman_reset(pacman_t* inout_pacman, cell_t in_start_cell)
{
    ASSERT(inout_pacman != NULL);

    agent_place(&inout_pacman->agent, in_start_cell, DIRECTION_NONE);

    inout_pacman->queued_direction = DIRECTION_NONE;
}

void pacman_set_intent(pacman_t* inout_pacman, direction_e in_direction)
{
    ASSERT(inout_pacman != NULL);

    if (in_direction == DIRECTION_NONE)
    {
        return;
    }

    inout_pacman->queued_direction = in_direction;
}

bool pacman_may_step(const pacman_t* in_pacman, const playfield_t* in_playfield, direction_e in_direction)
{
    ASSERT(in_pacman != NULL);
    ASSERT(in_playfield != NULL);

    if (!agent_can_step(&in_pacman->agent, in_playfield, in_direction))
    {
        return false;
    }

    return !playfield_is_gate(in_playfield, playfield_step(agent_get_cell(&in_pacman->agent), in_direction));
}

bool pacman_advance(pacman_t* inout_pacman, const playfield_t* in_playfield)
{
    ASSERT(inout_pacman != NULL);
    ASSERT(in_playfield != NULL);

    /* The queued direction is only taken up when it is actually possible; otherwise it
     * stays queued, so a turn asked for a moment too early still happens at the
     * junction rather than being thrown away. */
    if (pacman_may_step(inout_pacman, in_playfield, inout_pacman->queued_direction))
    {
        return agent_step(&inout_pacman->agent, in_playfield, inout_pacman->queued_direction);
    }

    if (pacman_may_step(inout_pacman, in_playfield, agent_get_direction(&inout_pacman->agent)))
    {
        return agent_step(&inout_pacman->agent, in_playfield, agent_get_direction(&inout_pacman->agent));
    }

    /* Stopped: he keeps the facing he already had (§10.1) and stays put. */
    return false;
}

cell_t pacman_get_cell(const pacman_t* in_pacman)
{
    ASSERT(in_pacman != NULL);

    return agent_get_cell(&in_pacman->agent);
}

direction_e pacman_get_direction(const pacman_t* in_pacman)
{
    ASSERT(in_pacman != NULL);

    return agent_get_direction(&in_pacman->agent);
}

cell_t pacman_get_cell_ahead(const pacman_t* in_pacman, uint8_t in_step_count)
{
    ASSERT(in_pacman != NULL);

    return agent_get_cell_ahead(&in_pacman->agent, in_step_count);
}
