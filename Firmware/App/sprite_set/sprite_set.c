#include "sprite_set.h"

#include <stddef.h>

#include "custom_assert.h"
#include "sprite.h"

/* ==========================================================================
 * sprite_set - private
 * ========================================================================= */

/* The art. One character per pixel: '.' shows what is behind, '1' the body colour, '2'
 * and '3' the two detail colours a palette supplies.
 *
 * Edit these by hand — that is what they are for. `clang-format off` keeps the formatter
 * from reflowing a picture into a paragraph, exactly as the maze in `playfield.c` does.
 *
 * They were first laid out by a throwaway script (a disc with a wedge removed, a dome
 * with three feet), because getting a circle right by eye in a 20 x 20 grid is tedious
 * and getting it right by arithmetic is not. The script is gone; these are the source
 * now. */

/* clang-format off */
static const char* const g_pacman_closed[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111111111111111111.",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    ".111111111111111111.",
    ".111111111111111111.",
    "..1111111111111111..",
    "..1111111111111111..",
    "...11111111111111...",
    ".....1111111111.....",
    ".......111111.......",
};

static const char* const g_pacman_open_east[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..111111111111111...",
    ".111111111111111....",
    ".1111111111111......",
    "1111111111111.......",
    "111111111111........",
    "11111111111.........",
    "11111111111.........",
    "111111111111........",
    "1111111111111.......",
    ".1111111111111......",
    ".111111111111111....",
    "..111111111111111...",
    "..1111111111111111..",
    "...11111111111111...",
    ".....1111111111.....",
    ".......111111.......",
};

static const char* const g_pacman_open_north[] = {
    "....................",
    "....................",
    "...1............1...",
    "..111..........111..",
    "..1111........1111..",
    ".11111........11111.",
    ".111111......111111.",
    "11111111....11111111",
    "111111111..111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    ".111111111111111111.",
    ".111111111111111111.",
    "..1111111111111111..",
    "..1111111111111111..",
    "...11111111111111...",
    ".....1111111111.....",
    ".......111111.......",
};

static const char* const g_pacman_open_west[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "...111111111111111..",
    "....111111111111111.",
    "......1111111111111.",
    ".......1111111111111",
    "........111111111111",
    ".........11111111111",
    ".........11111111111",
    "........111111111111",
    ".......1111111111111",
    "......1111111111111.",
    "....111111111111111.",
    "...111111111111111..",
    "..1111111111111111..",
    "...11111111111111...",
    ".....1111111111.....",
    ".......111111.......",
};

static const char* const g_pacman_open_south[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111111111111111111.",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "11111111111111111111",
    "111111111..111111111",
    "11111111....11111111",
    ".111111......111111.",
    ".11111........11111.",
    "..1111........1111..",
    "..111..........111..",
    "...1............1...",
    "....................",
    "....................",
};

static const char* const g_ghost_east[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111122221112222111.",
    "11111223311122331111",
    "11111223311122331111",
    "11111222211122221111",
    "11111222211122221111",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
};

static const char* const g_ghost_north[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111123321112332111.",
    "11111233211123321111",
    "11111222211122221111",
    "11111222211122221111",
    "11111222211122221111",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
};

static const char* const g_ghost_west[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111122221112222111.",
    "11111332211133221111",
    "11111332211133221111",
    "11111222211122221111",
    "11111222211122221111",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
};

static const char* const g_ghost_south[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111122221112222111.",
    "11111222211122221111",
    "11111233211123321111",
    "11111233211123321111",
    "11111222211122221111",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
};

static const char* const g_ghost_frightened[] = {
    ".......111111.......",
    ".....1111111111.....",
    "...11111111111111...",
    "..1111111111111111..",
    "..1111111111111111..",
    ".111111111111111111.",
    ".111111111111111111.",
    "11111111111111111111",
    "11111122211112221111",
    "11111122211112221111",
    "11111122211112221111",
    ".111111111111111111.",
    ".111111111111111111.",
    ".111121122112211111.",
    ".111111111111111111.",
    ".111111111111111111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
    ".1111...1111...1111.",
};

/* clang-format on */

#define SPRITE_SET_SIZE (20U)

static const sprite_t g_sprites[SPRITE_SET_ID_COUNT] = {
    [SPRITE_SET_PACMAN_CLOSED] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_pacman_closed},
    [SPRITE_SET_PACMAN_OPEN_EAST] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_pacman_open_east},
    [SPRITE_SET_PACMAN_OPEN_NORTH] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_pacman_open_north},
    [SPRITE_SET_PACMAN_OPEN_WEST] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_pacman_open_west},
    [SPRITE_SET_PACMAN_OPEN_SOUTH] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_pacman_open_south},
    [SPRITE_SET_GHOST_EAST] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_ghost_east},
    [SPRITE_SET_GHOST_NORTH] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_ghost_north},
    [SPRITE_SET_GHOST_WEST] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_ghost_west},
    [SPRITE_SET_GHOST_SOUTH] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_ghost_south},
    [SPRITE_SET_GHOST_FRIGHTENED] = {SPRITE_SET_SIZE, SPRITE_SET_SIZE, g_ghost_frightened},
};

/* One drawing, five palettes. Index 0 is never read — it is the transparent character's
 * slot, kept so the array index matches the digit in the art. */
static const sprite_palette_t g_palettes[SPRITE_SET_PALETTE_COUNT] = {
    [SPRITE_SET_PALETTE_PACMAN] = {{0U, FRAMEBUFFER_COLOR_YELLOW, 0U, 0U}},
    [SPRITE_SET_PALETTE_BLINKY] = {{0U, FRAMEBUFFER_RGB(255U, 0U, 0U), FRAMEBUFFER_COLOR_WHITE,
                                    FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_PINKY] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 255U), FRAMEBUFFER_COLOR_WHITE,
                                   FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_INKY] = {{0U, FRAMEBUFFER_RGB(0U, 255U, 255U), FRAMEBUFFER_COLOR_WHITE,
                                  FRAMEBUFFER_RGB(0U, 0U, 160U)}},
    [SPRITE_SET_PALETTE_CLYDE] = {{0U, FRAMEBUFFER_RGB(255U, 184U, 82U), FRAMEBUFFER_COLOR_WHITE,
                                   FRAMEBUFFER_RGB(0U, 0U, 160U)}},

    /* The one the four ghosts share when a power pellet is eaten: a dark blue body and a
     * white face, so which ghost it is stops mattering — which is the point. */
    [SPRITE_SET_PALETTE_FRIGHTENED] = {{0U, FRAMEBUFFER_RGB(33U, 33U, 255U), FRAMEBUFFER_COLOR_WHITE, 0U}},
};

/* ==========================================================================
 * sprite_set - public
 * ========================================================================= */

const sprite_t* sprite_set_get(sprite_set_id_e in_id)
{
    ASSERT(in_id < SPRITE_SET_ID_COUNT);

    return &g_sprites[in_id];
}

const sprite_palette_t* sprite_set_get_palette(sprite_set_palette_e in_palette)
{
    ASSERT(in_palette < SPRITE_SET_PALETTE_COUNT);

    return &g_palettes[in_palette];
}

sprite_set_id_e sprite_set_get_ghost_sprite(direction_e in_direction)
{
    switch (in_direction)
    {
        case DIRECTION_NORTH: return SPRITE_SET_GHOST_NORTH;

        case DIRECTION_SOUTH: return SPRITE_SET_GHOST_SOUTH;

        case DIRECTION_WEST: return SPRITE_SET_GHOST_WEST;

        default:
            /* Including DIRECTION_NONE: a ghost in the pen has to look somewhere. */
            return SPRITE_SET_GHOST_EAST;
    }
}

/* Pacman's mouth is the animation, so the frame depends on both where he faces and how
 * far through his step he is — closed at the ends, open in the middle. */
sprite_set_id_e sprite_set_get_pacman_sprite(direction_e in_direction, cell_progress_t in_progress)
{
    const bool is_mouth_open = (in_progress >= SPRITE_SET_MOUTH_OPENS_AT) && (in_progress < SPRITE_SET_MOUTH_CLOSES_AT);

    if (!is_mouth_open)
    {
        return SPRITE_SET_PACMAN_CLOSED;
    }

    switch (in_direction)
    {
        case DIRECTION_NORTH: return SPRITE_SET_PACMAN_OPEN_NORTH;

        case DIRECTION_SOUTH: return SPRITE_SET_PACMAN_OPEN_SOUTH;

        case DIRECTION_WEST: return SPRITE_SET_PACMAN_OPEN_WEST;

        default: return SPRITE_SET_PACMAN_OPEN_EAST;
    }
}
