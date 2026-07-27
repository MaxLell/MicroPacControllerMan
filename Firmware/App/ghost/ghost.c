#include "ghost.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "agent.h"
#include "custom_assert.h"
#include "ghost_path.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * ghost - private
 * ========================================================================= */

/* §10.4's tunable look-aheads and Clyde's shyness radius. */
#define PINKY_LOOK_AHEAD (2U)
#define INKY_LOOK_AHEAD (1U)
#define CLYDE_SHY_DISTANCE (4U)

static cell_t prv_cell_ahead(cell_t in_cell, direction_e in_direction, uint8_t in_step_count)
{
    cell_t ahead = in_cell;

    for (uint8_t step = 0U; step < in_step_count; ++step)
    {
        ahead = playfield_step(ahead, in_direction);
    }

    return ahead;
}

/* Inky's flank (§10.4): take the cell one ahead of Pacman, then extend the line from
 * Blinky through it by the same length again. The effect is that Inky aims at wherever
 * Pacman is heading *away* from Blinky, which is what makes the pair pincer. */
static cell_t prv_get_inky_target(cell_t in_pacman_cell, direction_e in_pacman_direction,
                                  cell_t in_blinky_cell)
{
    const cell_t pivot = prv_cell_ahead(in_pacman_cell, in_pacman_direction, INKY_LOOK_AHEAD);
    cell_t target;

    target.x = (int16_t)(pivot.x + (pivot.x - in_blinky_cell.x));
    target.y = (int16_t)(pivot.y + (pivot.y - in_blinky_cell.y));

    return target;
}

/* Clyde's shyness (§10.4): hunts like Blinky from a distance, but breaks off for his own
 * corner once he gets close, which is what makes him the one that lets you past. */
static cell_t prv_get_clyde_target(const ghost_t* const in_ghost, cell_t in_pacman_cell)
{
    const cell_t corner = playfield_get_scatter_corner((uint8_t)in_ghost->personality);

    if (playfield_get_distance(agent_get_cell(&in_ghost->agent), in_pacman_cell)
        > CLYDE_SHY_DISTANCE)
    {
        return in_pacman_cell;
    }

    return corner;
}

/* ==========================================================================
 * ghost - public
 * ========================================================================= */

void ghost_reset(ghost_t* inout_ghost, ghost_personality_e in_personality, cell_t in_pen_cell)
{
    ASSERT(inout_ghost != NULL);
    ASSERT(in_personality < GHOST_COUNT);

    agent_place(&inout_ghost->agent, in_pen_cell, DIRECTION_NONE);

    inout_ghost->personality = in_personality;
    inout_ghost->mode = GHOST_MODE_SCATTER;
    inout_ghost->may_reverse = false;
}

void ghost_send_to_pen(ghost_t* inout_ghost, cell_t in_pen_cell)
{
    ASSERT(inout_ghost != NULL);

    agent_place(&inout_ghost->agent, in_pen_cell, DIRECTION_NONE);

    /* Back to hunting from the pen. The caller decides which non-frightened mode the
     * others are in and will set it on the next mode update; scatter is the safe default
     * because it cannot make the ghost eat Pacman on the cell it reappeared in. */
    inout_ghost->mode = GHOST_MODE_SCATTER;
    inout_ghost->may_reverse = false;
}

void ghost_set_mode(ghost_t* inout_ghost, ghost_mode_e in_mode)
{
    ASSERT(inout_ghost != NULL);

    if (inout_ghost->mode == in_mode)
    {
        /* No change, so no free reversal — this is what lets a caller drive the mode every
         * tick without the ghosts jittering back and forth. */
        return;
    }

    inout_ghost->mode = in_mode;
    inout_ghost->may_reverse = true;
}

cell_t ghost_get_target(const ghost_t* in_ghost, cell_t in_pacman_cell,
                        direction_e in_pacman_direction, cell_t in_blinky_cell)
{
    ASSERT(in_ghost != NULL);

    if (in_ghost->mode == GHOST_MODE_SCATTER)
    {
        return playfield_get_scatter_corner((uint8_t)in_ghost->personality);
    }

    switch (in_ghost->personality)
    {
        case GHOST_BLINKY:
            return in_pacman_cell;
        case GHOST_PINKY:
            return prv_cell_ahead(in_pacman_cell, in_pacman_direction, PINKY_LOOK_AHEAD);
        case GHOST_INKY:
            return prv_get_inky_target(in_pacman_cell, in_pacman_direction, in_blinky_cell);
        case GHOST_CLYDE:
            return prv_get_clyde_target(in_ghost, in_pacman_cell);
        default:
            ASSERT(false);
            break;
    }

    return in_pacman_cell;
}

bool ghost_advance(ghost_t* inout_ghost, const playfield_t* in_playfield, cell_t in_pacman_cell,
                   direction_e in_pacman_direction, cell_t in_blinky_cell)
{
    direction_e forbidden;
    direction_e chosen;

    ASSERT(inout_ghost != NULL);
    ASSERT(in_playfield != NULL);

    /* Normally the way back is barred (§10.1); a mode change buys exactly one exemption,
     * spent here whether or not the ghost actually turns around. */
    forbidden = playfield_get_opposite_direction(agent_get_direction(&inout_ghost->agent));

    if (inout_ghost->may_reverse)
    {
        inout_ghost->may_reverse = false;
        forbidden = DIRECTION_NONE;
    }

    if (inout_ghost->mode == GHOST_MODE_FRIGHTENED)
    {
        chosen = ghost_path_find_step_away_from(in_playfield, agent_get_cell(&inout_ghost->agent),
                                                in_pacman_cell, forbidden);
    }
    else
    {
        const cell_t target
            = ghost_get_target(inout_ghost, in_pacman_cell, in_pacman_direction, in_blinky_cell);

        chosen = ghost_path_find_step_towards(in_playfield, agent_get_cell(&inout_ghost->agent),
                                              target, forbidden);
    }

    return agent_step(&inout_ghost->agent, in_playfield, chosen);
}

cell_t ghost_get_cell(const ghost_t* in_ghost)
{
    ASSERT(in_ghost != NULL);

    return agent_get_cell(&in_ghost->agent);
}

direction_e ghost_get_direction(const ghost_t* in_ghost)
{
    ASSERT(in_ghost != NULL);

    return agent_get_direction(&in_ghost->agent);
}

ghost_mode_e ghost_get_mode(const ghost_t* in_ghost)
{
    ASSERT(in_ghost != NULL);

    return in_ghost->mode;
}

bool ghost_is_frightened(const ghost_t* in_ghost)
{
    return ghost_get_mode(in_ghost) == GHOST_MODE_FRIGHTENED;
}
