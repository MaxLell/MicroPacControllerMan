#include "game_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "framebuffer.h"
#include "game.h"
#include "gfx.h"
#include "playfield.h"

/* ==========================================================================
 * game_view - private
 * ========================================================================= */

/* An 11x9 maze at 10 px a cell is 110x90, which leaves a strip at the bottom of the 128x128
 * panel for the HUD (§10.2's "centred, HUD in the remaining space"). */
#define TILE_SIZE (10)
#define MAZE_PIXEL_WIDTH (PLAYFIELD_WIDTH * TILE_SIZE)
#define MAZE_PIXEL_HEIGHT (PLAYFIELD_HEIGHT * TILE_SIZE)
#define MAZE_ORIGIN_X ((FRAMEBUFFER_WIDTH - MAZE_PIXEL_WIDTH) / 2)
#define MAZE_ORIGIN_Y (2)

#define HUD_ORIGIN_Y (MAZE_ORIGIN_Y + MAZE_PIXEL_HEIGHT + 4)

/* Entities are drawn inset from their cell so neighbouring ones stay visually separate. */
#define ENTITY_RADIUS (4)
#define PELLET_RADIUS (1)
#define POWER_PELLET_RADIUS (3)

/* A frightened ghost is drawn hollow, which reads as "different" on a 1-bpp panel where
 * there is no colour to change (FR-018). */
#define FRIGHTENED_RADIUS (4)

/* A 3x5 digit font, one byte per column, bit 0 at the top. Small enough to hand-write and
 * the only way to show a score without dragging in a font engine. */
#define DIGIT_WIDTH (3)
#define DIGIT_HEIGHT (5)
#define DIGIT_SPACING (1)
#define DIGIT_COUNT (10)

static const uint8_t k_digit_font[DIGIT_COUNT][DIGIT_WIDTH] = {
    {0x1FU, 0x11U, 0x1FU}, /* 0 */
    {0x00U, 0x1FU, 0x00U}, /* 1 */
    {0x1DU, 0x15U, 0x17U}, /* 2 */
    {0x11U, 0x15U, 0x1FU}, /* 3 */
    {0x07U, 0x04U, 0x1FU}, /* 4 */
    {0x17U, 0x15U, 0x1DU}, /* 5 */
    {0x1FU, 0x15U, 0x1DU}, /* 6 */
    {0x01U, 0x01U, 0x1FU}, /* 7 */
    {0x1FU, 0x15U, 0x1FU}, /* 8 */
    {0x17U, 0x15U, 0x1FU}  /* 9 */
};

#define SCORE_MAX_DIGITS (6U)

static int16_t prv_cell_origin_x(int16_t in_cell_x)
{
    return (int16_t)(MAZE_ORIGIN_X + (in_cell_x * TILE_SIZE));
}

static int16_t prv_cell_origin_y(int16_t in_cell_y)
{
    return (int16_t)(MAZE_ORIGIN_Y + (in_cell_y * TILE_SIZE));
}

static int16_t prv_cell_centre_x(int16_t in_cell_x)
{
    return (int16_t)(prv_cell_origin_x(in_cell_x) + (TILE_SIZE / 2));
}

static int16_t prv_cell_centre_y(int16_t in_cell_y)
{
    return (int16_t)(prv_cell_origin_y(in_cell_y) + (TILE_SIZE / 2));
}

/* The maze walls come from the playfield rather than the snapshot: they are static for a
 * level, so copying them into every frame would be 99 bytes of nothing. */
static void prv_draw_maze(framebuffer_t* const inout_framebuffer,
                          const playfield_t* const in_playfield)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};

            if (playfield_is_walkable(in_playfield, cell))
            {
                continue;
            }

            gfx_filled_rectangle(inout_framebuffer, prv_cell_origin_x(x), prv_cell_origin_y(y),
                                 TILE_SIZE, TILE_SIZE, FRAMEBUFFER_COLOR_BLACK);
        }
    }
}

static void prv_draw_pellets(framebuffer_t* const inout_framebuffer,
                             const game_snapshot_t* const in_snapshot)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const int16_t centre_x = prv_cell_centre_x(x);
            const int16_t centre_y = prv_cell_centre_y(y);

            if (in_snapshot->pellets[y][x] == PLAYFIELD_PELLET_NORMAL)
            {
                gfx_filled_circle(inout_framebuffer, centre_x, centre_y, PELLET_RADIUS,
                                  FRAMEBUFFER_COLOR_BLACK);
            }
            else if (in_snapshot->pellets[y][x] == PLAYFIELD_PELLET_POWER)
            {
                gfx_filled_circle(inout_framebuffer, centre_x, centre_y, POWER_PELLET_RADIUS,
                                  FRAMEBUFFER_COLOR_BLACK);
            }
            else
            {
                /* Nothing left on this cell. */
            }
        }
    }
}

/* Pacman as a disc with a wedge bitten out of it, facing the way he is going — the mouth is
 * what makes his direction readable at 10 px without any colour. */
static void prv_draw_pacman(framebuffer_t* const inout_framebuffer,
                            const game_snapshot_t* const in_snapshot)
{
    const int16_t centre_x = prv_cell_centre_x(in_snapshot->pacman_cell.x);
    const int16_t centre_y = prv_cell_centre_y(in_snapshot->pacman_cell.y);
    int16_t mouth_x = centre_x;
    int16_t mouth_y = centre_y;

    gfx_filled_circle(inout_framebuffer, centre_x, centre_y, ENTITY_RADIUS,
                      FRAMEBUFFER_COLOR_BLACK);

    switch (in_snapshot->pacman_direction)
    {
        case DIRECTION_NORTH:
            mouth_y = (int16_t)(centre_y - ENTITY_RADIUS);
            break;
        case DIRECTION_SOUTH:
            mouth_y = (int16_t)(centre_y + ENTITY_RADIUS);
            break;
        case DIRECTION_WEST:
            mouth_x = (int16_t)(centre_x - ENTITY_RADIUS);
            break;
        case DIRECTION_EAST:
            mouth_x = (int16_t)(centre_x + ENTITY_RADIUS);
            break;
        default:
            /* Standing still: no mouth, just a disc. */
            return;
    }

    gfx_filled_triangle(inout_framebuffer, centre_x, centre_y, mouth_x, mouth_y,
                        (mouth_x == centre_x) ? (int16_t)(centre_x + ENTITY_RADIUS) : mouth_x,
                        (mouth_x == centre_x) ? mouth_y : (int16_t)(centre_y + ENTITY_RADIUS),
                        FRAMEBUFFER_COLOR_WHITE);
}

static void prv_draw_ghosts(framebuffer_t* const inout_framebuffer,
                            const game_snapshot_t* const in_snapshot)
{
    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        const int16_t centre_x = prv_cell_centre_x(in_snapshot->ghost_cells[index].x);
        const int16_t centre_y = prv_cell_centre_y(in_snapshot->ghost_cells[index].y);

        if (in_snapshot->ghost_is_frightened[index])
        {
            gfx_circle(inout_framebuffer, centre_x, centre_y, FRIGHTENED_RADIUS,
                       FRAMEBUFFER_COLOR_BLACK);

            continue;
        }

        /* A solid square, so a ghost never reads as Pacman even at a glance. */
        gfx_filled_rectangle(inout_framebuffer, (int16_t)(centre_x - ENTITY_RADIUS),
                             (int16_t)(centre_y - ENTITY_RADIUS), (ENTITY_RADIUS * 2),
                             (ENTITY_RADIUS * 2), FRAMEBUFFER_COLOR_BLACK);
    }
}

static void prv_draw_digit(framebuffer_t* const inout_framebuffer, int16_t in_x, int16_t in_y,
                           uint8_t in_digit)
{
    ASSERT(in_digit < DIGIT_COUNT);

    for (int16_t column = 0; column < DIGIT_WIDTH; ++column)
    {
        const uint8_t bits = k_digit_font[in_digit][column];

        for (int16_t row = 0; row < DIGIT_HEIGHT; ++row)
        {
            if ((bits & (1U << row)) != 0U)
            {
                framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_x + column),
                                      (int16_t)(in_y + row), FRAMEBUFFER_COLOR_BLACK);
            }
        }
    }
}

/* Right-aligned, so the score does not shift about as it grows. */
static void prv_draw_number(framebuffer_t* const inout_framebuffer, int16_t in_right_x,
                            int16_t in_y, uint32_t in_value)
{
    uint32_t remaining = in_value;
    int16_t x = in_right_x;
    uint8_t digits_drawn = 0U;

    do
    {
        prv_draw_digit(inout_framebuffer, (int16_t)(x - DIGIT_WIDTH), in_y,
                       (uint8_t)(remaining % 10U));

        x = (int16_t)(x - (DIGIT_WIDTH + DIGIT_SPACING));
        remaining /= 10U;
        ++digits_drawn;
    } while ((remaining > 0U) && (digits_drawn < SCORE_MAX_DIGITS));
}

static void prv_draw_hud(framebuffer_t* const inout_framebuffer,
                         const game_snapshot_t* const in_snapshot)
{
    /* Level on the left, one pip per remaining life beside it, score on the right. */
    prv_draw_number(inout_framebuffer, (int16_t)(MAZE_ORIGIN_X + (DIGIT_WIDTH * 2)), HUD_ORIGIN_Y,
                    in_snapshot->level);

    for (uint8_t life = 0U; life < in_snapshot->lives; ++life)
    {
        gfx_filled_circle(inout_framebuffer,
                          (int16_t)(MAZE_ORIGIN_X + 14 + (life * (PELLET_RADIUS * 4))),
                          (int16_t)(HUD_ORIGIN_Y + 2), PELLET_RADIUS, FRAMEBUFFER_COLOR_BLACK);
    }

    prv_draw_number(inout_framebuffer, (int16_t)(MAZE_ORIGIN_X + MAZE_PIXEL_WIDTH), HUD_ORIGIN_Y,
                    in_snapshot->score);
}

/* ==========================================================================
 * game_view - public
 * ========================================================================= */

void game_view_draw(framebuffer_t* inout_framebuffer, const game_snapshot_t* in_snapshot)
{
    playfield_t playfield;

    ASSERT(inout_framebuffer != NULL);
    ASSERT(in_snapshot != NULL);

    /* The snapshot carries the pellets, which change, but not the walls, which do not — so
     * the level's static layout is reloaded here rather than copied into every frame. */
    playfield_load_level(&playfield, in_snapshot->level);

    framebuffer_clear(inout_framebuffer);

    prv_draw_maze(inout_framebuffer, &playfield);
    prv_draw_pellets(inout_framebuffer, in_snapshot);
    prv_draw_ghosts(inout_framebuffer, in_snapshot);
    prv_draw_pacman(inout_framebuffer, in_snapshot);
    prv_draw_hud(inout_framebuffer, in_snapshot);
}
