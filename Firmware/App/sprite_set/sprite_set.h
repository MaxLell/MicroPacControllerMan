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

/*! \brief Where in a step Pacman's mouth reaches each of its three positions, in the
 *         1/256ths of \ref cell_progress_t.
 *
 * The arcade chews **shut, half, wide, half** and repeats. Here that cycle is clocked by
 * the step rather than by a free-running frame counter, so a slower Pacman chews more
 * slowly and the mouth cannot drift against the movement. */
#define SPRITE_SET_MOUTH_HALF_AT   (64U)
#define SPRITE_SET_MOUTH_WIDE_AT   (128U)
#define SPRITE_SET_MOUTH_CLOSES_AT (192U)

/*! \brief Where in a step a ghost swaps its skirt over. Two frames, half a cell each. */
#define SPRITE_SET_SKIRT_SWAPS_AT  (128U)

typedef enum
{
    /* Pacman: one closed frame shared by every direction, then half and wide per
     * direction — the arcade's own three-frame chew. */
    SPRITE_SET_PACMAN_CLOSED = 0,
    SPRITE_SET_PACMAN_HALF_EAST,
    SPRITE_SET_PACMAN_HALF_NORTH,
    SPRITE_SET_PACMAN_HALF_WEST,
    SPRITE_SET_PACMAN_HALF_SOUTH,
    SPRITE_SET_PACMAN_WIDE_EAST,
    SPRITE_SET_PACMAN_WIDE_NORTH,
    SPRITE_SET_PACMAN_WIDE_WEST,
    SPRITE_SET_PACMAN_WIDE_SOUTH,

    /* Ghosts: the eyes give the direction, and the two frames per direction are the
     * skirt waving. Without the second frame a ghost reads as a sticker being slid
     * across the maze. */
    SPRITE_SET_GHOST_EAST_A,
    SPRITE_SET_GHOST_EAST_B,
    SPRITE_SET_GHOST_NORTH_A,
    SPRITE_SET_GHOST_NORTH_B,
    SPRITE_SET_GHOST_WEST_A,
    SPRITE_SET_GHOST_WEST_B,
    SPRITE_SET_GHOST_SOUTH_A,
    SPRITE_SET_GHOST_SOUTH_B,

    /* Frightened has no direction — the eyes go square and the mouth zigzags. */
    SPRITE_SET_GHOST_FRIGHTENED_A,
    SPRITE_SET_GHOST_FRIGHTENED_B,

    /* The field. One tile drawing plus a palette is an empty cell or a wall; the two
     * pellet drawings carry their own black surroundings so a cell is one item. */
    SPRITE_SET_TILE,
    SPRITE_SET_TILE_PELLET,
    SPRITE_SET_TILE_POWER_PELLET,

    /*! An actor-sized blank, so a spent life can be wiped from the HUD in one item rather
     *  than four tiles. */
    SPRITE_SET_ACTOR_BLANK,

    /* The font. Contiguous and in ASCII order within each run, which is what lets
     * #sprite_set_get_glyph be arithmetic. */
    SPRITE_SET_GLYPH_SPACE,
    SPRITE_SET_GLYPH_DIGIT_0,
    SPRITE_SET_GLYPH_DIGIT_1,
    SPRITE_SET_GLYPH_DIGIT_2,
    SPRITE_SET_GLYPH_DIGIT_3,
    SPRITE_SET_GLYPH_DIGIT_4,
    SPRITE_SET_GLYPH_DIGIT_5,
    SPRITE_SET_GLYPH_DIGIT_6,
    SPRITE_SET_GLYPH_DIGIT_7,
    SPRITE_SET_GLYPH_DIGIT_8,
    SPRITE_SET_GLYPH_DIGIT_9,
    SPRITE_SET_GLYPH_A,
    SPRITE_SET_GLYPH_B,
    SPRITE_SET_GLYPH_C,
    SPRITE_SET_GLYPH_D,
    SPRITE_SET_GLYPH_E,
    SPRITE_SET_GLYPH_F,
    SPRITE_SET_GLYPH_G,
    SPRITE_SET_GLYPH_H,
    SPRITE_SET_GLYPH_I,
    SPRITE_SET_GLYPH_J,
    SPRITE_SET_GLYPH_K,
    SPRITE_SET_GLYPH_L,
    SPRITE_SET_GLYPH_M,
    SPRITE_SET_GLYPH_N,
    SPRITE_SET_GLYPH_O,
    SPRITE_SET_GLYPH_P,
    SPRITE_SET_GLYPH_Q,
    SPRITE_SET_GLYPH_R,
    SPRITE_SET_GLYPH_S,
    SPRITE_SET_GLYPH_T,
    SPRITE_SET_GLYPH_U,
    SPRITE_SET_GLYPH_V,
    SPRITE_SET_GLYPH_W,
    SPRITE_SET_GLYPH_X,
    SPRITE_SET_GLYPH_Y,
    SPRITE_SET_GLYPH_Z,

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
    SPRITE_SET_PALETTE_FRIGHTENED_FLASH, /*!< The warning that the window is closing */
    SPRITE_SET_PALETTE_EMPTY,
    SPRITE_SET_PALETTE_WALL,
    SPRITE_SET_PALETTE_DOOR, /*!< The ghost house gate, which is not blue like the rest */
    SPRITE_SET_PALETTE_TEXT, /*!< The HUD, and every screen made of words                */
    SPRITE_SET_PALETTE_PELLET,
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

/*! \brief Which ghost drawing faces a given way, at this point in its step.
 *
 * \param[in]       in_direction: where the ghost is heading; `NONE` looks east
 * \param[in]       in_progress: how far through the step, from \ref msg_actor_t — it is
 *                      what clocks the skirt
 * \return          A `SPRITE_SET_GHOST_*` id
 */
sprite_set_id_e sprite_set_get_ghost_sprite(direction_e in_direction, cell_progress_t in_progress);

/*! \brief Which frightened-ghost drawing to use at this point in the step.
 *
 * No direction: a frightened ghost looks the same whichever way it is running.
 */
sprite_set_id_e sprite_set_get_frightened_sprite(cell_progress_t in_progress);

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

/*! \brief The drawing for a character of the HUD's alphabet.
 *
 * Digits, capital letters and the space. Anything else is a programming error rather than
 * a runtime condition — the strings this draws are all in the source.
 *
 * \param[in]       in_character: `'0'`..`'9'`, `'A'`..`'Z'` or `' '`
 * \return          A `SPRITE_SET_GLYPH_*` id
 */
sprite_set_id_e sprite_set_get_glyph(char in_character);

#endif /* SPRITE_SET_H */
