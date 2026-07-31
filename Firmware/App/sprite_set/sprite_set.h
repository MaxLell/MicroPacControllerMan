/*
 * sprite_set.h
 *
 * The game's drawings and the colours they are drawn in: Pacman with his mouth, the
 * ghost, and the palettes that turn one ghost drawing into four ghosts plus the
 * frightened one.
 *
 * Plain read-only data with the two lookups that go with it. It sits in `App/` because
 * it is *this game's* art, while `Services/sprite` is the reusable primitive that knows
 * how to draw any of it — the same split as `switch` and `user_button`.
 *
 * Deliberately **not** inside the render port: the SDL view on the host has to draw the
 * same figures as the panel, or the host build stops being evidence about the device.
 */

#ifndef SPRITE_SET_H
#define SPRITE_SET_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"
#include "sprite.h"

/* ==========================================================================
 * sprite_set - public types
 * ========================================================================= */

/*! \brief Progress through a step at which Pacman's mouth opens and shuts again, in the
 *         1/256ths of \ref cell_progress_t. Open in the middle of a step and closed at
 *         both ends, so the chewing lines up with the movement rather than free-running
 *         against it. */
#define SPRITE_SET_MOUTH_OPENS_AT  (64U)
#define SPRITE_SET_MOUTH_CLOSES_AT (192U)

typedef enum
{
    SPRITE_SET_PACMAN_CLOSED = 0,
    SPRITE_SET_PACMAN_OPEN_EAST,
    SPRITE_SET_PACMAN_OPEN_NORTH,
    SPRITE_SET_PACMAN_OPEN_WEST,
    SPRITE_SET_PACMAN_OPEN_SOUTH,
    SPRITE_SET_GHOST_EAST,
    SPRITE_SET_GHOST_NORTH,
    SPRITE_SET_GHOST_WEST,
    SPRITE_SET_GHOST_SOUTH,
    SPRITE_SET_GHOST_FRIGHTENED,
    SPRITE_SET_ID_COUNT
} sprite_set_id_e;

/*! \brief The colour schemes. The four ghosts differ only by which of these is used with
 *         the same drawing. */
typedef enum
{
    SPRITE_SET_PALETTE_PACMAN = 0,
    SPRITE_SET_PALETTE_BLINKY,
    SPRITE_SET_PALETTE_PINKY,
    SPRITE_SET_PALETTE_INKY,
    SPRITE_SET_PALETTE_CLYDE,
    SPRITE_SET_PALETTE_FRIGHTENED,
    SPRITE_SET_PALETTE_COUNT
} sprite_set_palette_e;

/* ==========================================================================
 * sprite_set - public API
 * ========================================================================= */

/*! \brief The drawing for an id.
 *
 * \param[in]       in_id: below \ref SPRITE_SET_ID_COUNT
 * \return          The sprite, never `NULL` and owned by this module
 */
const sprite_t* sprite_set_get(sprite_set_id_e in_id);

/*! \brief The colours for a palette.
 *
 * \param[in]       in_palette: below \ref SPRITE_SET_PALETTE_COUNT
 * \return          The palette, never `NULL` and owned by this module
 */
const sprite_palette_t* sprite_set_get_palette(sprite_set_palette_e in_palette);

/*! \brief Which ghost drawing faces a given way.
 *
 * \param[in]       in_direction: where the ghost is heading; `NONE` looks east
 * \return          A `SPRITE_SET_GHOST_*` id
 */
sprite_set_id_e sprite_set_get_ghost_sprite(direction_e in_direction);

/*! \brief Which Pacman drawing to use, given where he faces and how far through his
 *         step he is.
 *
 * The mouth is the animation and the step is its clock, so a slower Pacman chews more
 * slowly instead of the two drifting against each other.
 *
 * \param[in]       in_direction: where he faces; `NONE` looks east
 * \param[in]       in_progress: how far through the step, from \ref msg_actor_t
 * \return          A `SPRITE_SET_PACMAN_*` id
 */
sprite_set_id_e sprite_set_get_pacman_sprite(direction_e in_direction, cell_progress_t in_progress);

#endif /* SPRITE_SET_H */
