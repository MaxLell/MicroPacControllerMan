/*
 * ghost.h
 *
 * One ghost: an #agent_t plus the personality that decides what it aims at
 * ([10 §10.4](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)/§10.5, FR-014/015/018/019).
 *
 * The split against #ghost_path is deliberate and is the whole design here: **this module
 * owns the data and the targeting, that one owns the algorithm.** Each ghost works out
 * its own target cell from its personality and the current mode, then asks the shared
 * stateless library how to get one step closer. Swapping that library for A* touches
 * nothing in here.
 *
 * A ghost is given Pacman's cell and facing rather than a `pacman_t`, so it does not
 * depend on the player module at all — §10.4's targets need nothing more than that, plus
 * Blinky's cell for Inky's flanking rule.
 */

#ifndef GHOST_H
#define GHOST_H

#include <stdbool.h>
#include <stdint.h>

#include "agent.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * ghost - public types
 * ========================================================================= */

/*! \brief The four personalities of §10.4, in the order their scatter corners are
 *         assigned. */
typedef enum
{
    GHOST_BLINKY = 0,                           /*!< Direct: aims at Pacman            */
    GHOST_PINKY,                                /*!< Ambush: aims ahead of Pacman      */
    GHOST_INKY,                                 /*!< Flank: aims past Pacman from Blinky */
    GHOST_CLYDE,                                /*!< Shy: backs off when close         */
    GHOST_COUNT
} ghost_personality_e;

typedef enum
{
    GHOST_MODE_SCATTER = 0,                     /*!< Heads for its own corner          */
    GHOST_MODE_CHASE,                           /*!< Hunts, per its personality        */
    GHOST_MODE_FRIGHTENED                       /*!< Flees Pacman, edible (§10.5)      */
} ghost_mode_e;

typedef struct
{
    agent_t agent;
    ghost_personality_e personality;
    ghost_mode_e mode;
    bool may_reverse;                           /*!< Earned by a mode change (§10.1)   */
} ghost_t;

/* ==========================================================================
 * ghost - public API
 * ========================================================================= */

/*! \brief Put a ghost in the pen, facing nowhere, in scatter mode.
 *
 * \param[out]      inout_ghost: instance to reset, must not be `NULL`
 * \param[in]       in_personality: which ghost this is
 * \param[in]       in_pen_cell: from #playfield_get_pen_cell
 */
void ghost_reset(ghost_t* inout_ghost, ghost_personality_e in_personality, cell_t in_pen_cell);

/*! \brief Send an eaten ghost back to the pen (§10.5).
 *
 * It resumes normal behaviour from there; being eaten costs Pacman nothing.
 *
 * \param[in,out]   inout_ghost: instance
 * \param[in]       in_pen_cell: where to reappear
 */
void ghost_send_to_pen(ghost_t* inout_ghost, cell_t in_pen_cell);

/*! \brief Change mode, which earns the ghost one reversal.
 *
 * §10.1 lets a ghost turn around exactly when its mode changes — scatter↔chase, or
 * entering and leaving frightened. Setting the mode it is already in changes nothing, so
 * a caller may drive this every tick without granting free reversals.
 *
 * \param[in,out]   inout_ghost: instance
 * \param[in]       in_mode: the mode to enter
 */
void ghost_set_mode(ghost_t* inout_ghost, ghost_mode_e in_mode);

/*! \brief The cell this ghost is currently aiming at (§10.4).
 *
 * Exposed because it is the interesting half of the behaviour and worth testing directly.
 * The target may be a wall or outside the maze — Pinky's and Inky's routinely are. It is
 * only ever something to measure against.
 *
 * Meaningless in frightened mode, which has no target: the ghost simply runs away.
 *
 * \param[in]       in_ghost: instance
 * \param[in]       in_pacman_cell: where Pacman is
 * \param[in]       in_pacman_direction: which way he faces
 * \param[in]       in_blinky_cell: Blinky's cell, for Inky's rule
 * \return          The target cell
 */
cell_t ghost_get_target(const ghost_t* in_ghost, cell_t in_pacman_cell,
                        direction_e in_pacman_direction, cell_t in_blinky_cell);

/*! \brief Move the ghost one cell.
 *
 * Works out the target for the current mode, asks #ghost_path for the step, and takes it.
 * Consumes the reversal earned by a mode change, if any.
 *
 * \param[in,out]   inout_ghost: instance
 * \param[in]       in_playfield: the maze
 * \param[in]       in_pacman_cell: where Pacman is
 * \param[in]       in_pacman_direction: which way he faces
 * \param[in]       in_blinky_cell: Blinky's cell, for Inky's rule
 * \return          `true` when the ghost moved
 */
bool ghost_advance(ghost_t* inout_ghost, const playfield_t* in_playfield, cell_t in_pacman_cell,
                   direction_e in_pacman_direction, cell_t in_blinky_cell);

/*! \brief The cell the ghost occupies. */
cell_t ghost_get_cell(const ghost_t* in_ghost);

/*! \brief The direction the ghost faces. */
direction_e ghost_get_direction(const ghost_t* in_ghost);

/*! \brief The ghost's current mode. */
ghost_mode_e ghost_get_mode(const ghost_t* in_ghost);

/*! \brief Whether the ghost can currently be eaten rather than kill (§10.5). */
bool ghost_is_frightened(const ghost_t* in_ghost);

#endif /* GHOST_H */
