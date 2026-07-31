#include "agent.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * agent - public
 * ========================================================================= */

void agent_place(agent_t* inout_agent, cell_t in_cell, direction_e in_direction)
{
    ASSERT(inout_agent != NULL);

    inout_agent->cell = playfield_wrap_cell(in_cell);
    inout_agent->direction = in_direction;
}

bool agent_can_step(const agent_t* in_agent, const playfield_t* in_playfield, direction_e in_direction)
{
    ASSERT(in_agent != NULL);
    ASSERT(in_playfield != NULL);

    if (in_direction == DIRECTION_NONE)
    {
        return false;
    }

    return playfield_is_walkable(in_playfield, playfield_step(in_agent->cell, in_direction));
}

bool agent_step(agent_t* inout_agent, const playfield_t* in_playfield, direction_e in_direction)
{
    ASSERT(inout_agent != NULL);
    ASSERT(in_playfield != NULL);

    if (in_direction == DIRECTION_NONE)
    {
        return false;
    }

    /* Face the way we were told even if the wall stops us: an entity pressed against a
     * wall keeps pointing at it, and moves the moment the wall is no longer in the way. */
    inout_agent->direction = in_direction;

    if (!agent_can_step(inout_agent, in_playfield, in_direction))
    {
        return false;
    }

    inout_agent->cell = playfield_step(inout_agent->cell, in_direction);

    return true;
}

cell_t agent_get_cell(const agent_t* in_agent)
{
    ASSERT(in_agent != NULL);

    return in_agent->cell;
}

direction_e agent_get_direction(const agent_t* in_agent)
{
    ASSERT(in_agent != NULL);

    return in_agent->direction;
}

cell_t agent_get_cell_ahead(const agent_t* in_agent, uint8_t in_step_count)
{
    cell_t ahead;

    ASSERT(in_agent != NULL);

    ahead = in_agent->cell;

    for (uint8_t step = 0U; step < in_step_count; ++step)
    {
        ahead = playfield_step(ahead, in_agent->direction);
    }

    return ahead;
}
