/*
 * Unit tests for App/game_view.
 *
 * The View is the one part of the game with no observable behaviour except pixels, so it is
 * the easiest place for a mistake to survive: a maze drawn one cell off, a frightened ghost
 * that looks exactly like a hunting one, a HUD that never changes. On a 1-bpp panel there is
 * no colour to fall back on, so these check the few distinctions the picture has to carry
 * (FR-018) rather than trying to pin down every pixel.
 *
 * Reading the buffer back through #framebuffer_get_pixel means the tests describe what a
 * player would see, not how the bits are packed.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Ceedling links from this file's includes only, not transitively. */
#include "active_object.h"
#include "agent.h"
#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "game.h"
#include "game_view.h"
#include "gfx.h"
#include "ghost.h"
#include "ghost_path.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "pacman.h"
#include "playfield.h"
#include "score.h"
#include "unity.h"

/* Mirrors game_view's own layout constants. Duplicated on purpose: if the picture moves,
 * these tests should say so rather than move with it. */
#define TILE_SIZE      (10)
#define MAZE_ORIGIN_X  ((FRAMEBUFFER_WIDTH - (PLAYFIELD_WIDTH * TILE_SIZE)) / 2)
#define MAZE_ORIGIN_Y  (2)
#define HUD_ORIGIN_Y   (MAZE_ORIGIN_Y + (PLAYFIELD_HEIGHT * TILE_SIZE) + 4)

/* Below the HUD and right of the maze — nothing is ever drawn here. */
#define UNUSED_PIXEL_X (FRAMEBUFFER_WIDTH - 1)
#define UNUSED_PIXEL_Y (FRAMEBUFFER_HEIGHT - 1)

/* Level 1 of §10.2: the outer ring is wall, row 3 is open all the way across. */
#define A_WALL_CELL_X  (0)
#define A_WALL_CELL_Y  (0)
#define OPEN_ROW_Y     (3)
#define PACMAN_CELL_X  (5)

/* Two cells apart, so neighbouring entities cannot bleed into each other's centres. */
static const int16_t k_ghost_cell_x[GHOST_COUNT] = {1, 3, 7, 9};

static framebuffer_t g_framebuffer;
static game_snapshot_t g_snapshot;

static int16_t prv_cell_centre_x(int16_t in_cell_x)
{
    return (int16_t)(MAZE_ORIGIN_X + (in_cell_x * TILE_SIZE) + (TILE_SIZE / 2));
}

static int16_t prv_cell_centre_y(int16_t in_cell_y)
{
    return (int16_t)(MAZE_ORIGIN_Y + (in_cell_y * TILE_SIZE) + (TILE_SIZE / 2));
}

static bool prv_is_ink(int16_t in_x, int16_t in_y)
{
    return framebuffer_get_pixel(&g_framebuffer, in_x, in_y) == FRAMEBUFFER_COLOR_BLACK;
}

/* Ink anywhere in the strip the HUD occupies. */
static uint16_t prv_count_hud_ink(void)
{
    uint16_t count = 0U;

    for (int16_t y = HUD_ORIGIN_Y; y < (HUD_ORIGIN_Y + 5); ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            if (prv_is_ink(x, y))
            {
                ++count;
            }
        }
    }

    return count;
}

/* An empty level-1 maze with everyone parked on the open middle row — the fixture the
 * per-entity tests vary one thing at a time from. */
void setUp(void)
{
    assert_probe_begin();

    memset(&g_framebuffer, 0, sizeof(g_framebuffer));
    memset(&g_snapshot, 0, sizeof(g_snapshot));

    g_snapshot.version = 1U;
    g_snapshot.level = PLAYFIELD_FIRST_LEVEL;
    g_snapshot.lives = GAME_STARTING_LIVES;
    g_snapshot.score = 0U;
    g_snapshot.state = GAME_STATE_RUNNING;
    g_snapshot.pacman_cell.x = PACMAN_CELL_X;
    g_snapshot.pacman_cell.y = OPEN_ROW_Y;
    g_snapshot.pacman_direction = DIRECTION_NONE;

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        g_snapshot.ghost_cells[index].x = k_ghost_cell_x[index];
        g_snapshot.ghost_cells[index].y = OPEN_ROW_Y;
    }
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- the frame ----------------------------------------------------------- */

void test_drawing_starts_from_a_blank_buffer(void)
{
    gfx_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);

    game_view_draw(&g_framebuffer, &g_snapshot);

    /* Whatever was in the buffer before must be gone, not drawn over. */
    TEST_ASSERT_FALSE(prv_is_ink(UNUSED_PIXEL_X, UNUSED_PIXEL_Y));
}

void test_the_maze_walls_are_drawn(void)
{
    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(A_WALL_CELL_X), prv_cell_centre_y(A_WALL_CELL_Y)));
}

void test_an_open_cell_is_left_empty(void)
{
    /* Nothing on this cell and nobody standing on it: it must stay background, or the maze
     * is drawn a cell out of step. */
    const int16_t empty_cell_x = 5;

    g_snapshot.pacman_cell.x = 1;
    g_snapshot.pacman_cell.y = 1;

    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_FALSE(prv_is_ink(prv_cell_centre_x(empty_cell_x), prv_cell_centre_y(OPEN_ROW_Y)));
}

/* --- pellets ------------------------------------------------------------- */

void test_a_pellet_is_drawn_where_the_snapshot_says(void)
{
    const int16_t pellet_cell_x = 4;

    g_snapshot.pellets[OPEN_ROW_Y][pellet_cell_x] = PLAYFIELD_PELLET_NORMAL;

    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(pellet_cell_x), prv_cell_centre_y(OPEN_ROW_Y)));
}

void test_a_power_pellet_is_drawn_bigger_than_a_normal_one(void)
{
    const int16_t normal_cell_x = 2;
    const int16_t power_cell_x = 4;
    const int16_t offset = 2;

    g_snapshot.pellets[OPEN_ROW_Y][normal_cell_x] = PLAYFIELD_PELLET_NORMAL;
    g_snapshot.pellets[OPEN_ROW_Y][power_cell_x] = PLAYFIELD_PELLET_POWER;

    game_view_draw(&g_framebuffer, &g_snapshot);

    /* Two pixels off centre: inside the power pellet, outside the normal one — which is the
     * whole point of showing them differently. */
    TEST_ASSERT_TRUE(prv_is_ink((int16_t)(prv_cell_centre_x(power_cell_x) + offset), prv_cell_centre_y(OPEN_ROW_Y)));
    TEST_ASSERT_FALSE(prv_is_ink((int16_t)(prv_cell_centre_x(normal_cell_x) + offset), prv_cell_centre_y(OPEN_ROW_Y)));
}

void test_an_eaten_cell_shows_nothing(void)
{
    const int16_t eaten_cell_x = 4;

    g_snapshot.pellets[OPEN_ROW_Y][eaten_cell_x] = PLAYFIELD_PELLET_NONE;

    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_FALSE(prv_is_ink(prv_cell_centre_x(eaten_cell_x), prv_cell_centre_y(OPEN_ROW_Y)));
}

/* --- the entities (FR-018) ----------------------------------------------- */

void test_pacman_is_drawn_on_his_cell(void)
{
    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(PACMAN_CELL_X), prv_cell_centre_y(OPEN_ROW_Y)));
}

void test_pacman_shows_which_way_he_is_facing(void)
{
    framebuffer_t facing_east;
    bool differs = false;

    g_snapshot.pacman_direction = DIRECTION_EAST;
    game_view_draw(&g_framebuffer, &g_snapshot);
    facing_east = g_framebuffer;

    g_snapshot.pacman_direction = DIRECTION_WEST;
    game_view_draw(&g_framebuffer, &g_snapshot);

    /* The mouth is the only thing that makes a direction readable at ten pixels, so the two
     * pictures have to differ. */
    differs = memcmp(&facing_east, &g_framebuffer, sizeof(framebuffer_t)) != 0;

    TEST_ASSERT_TRUE(differs);
}

void test_a_hunting_ghost_is_drawn_solid(void)
{
    g_snapshot.ghost_is_frightened[GHOST_BLINKY] = false;

    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(k_ghost_cell_x[GHOST_BLINKY]), prv_cell_centre_y(OPEN_ROW_Y)));
}

void test_a_frightened_ghost_is_drawn_hollow(void)
{
    const int16_t centre_x = prv_cell_centre_x(k_ghost_cell_x[GHOST_BLINKY]);
    const int16_t centre_y = prv_cell_centre_y(OPEN_ROW_Y);

    g_snapshot.ghost_is_frightened[GHOST_BLINKY] = true;

    game_view_draw(&g_framebuffer, &g_snapshot);

    /* Hollow, not absent: an outline where the solid block would be. Without colour this is
     * the only thing telling the player the ghost is edible. */
    TEST_ASSERT_FALSE(prv_is_ink(centre_x, centre_y));
    TEST_ASSERT_TRUE(prv_is_ink(centre_x, (int16_t)(centre_y - 4)));
}

void test_every_ghost_is_drawn(void)
{
    game_view_draw(&g_framebuffer, &g_snapshot);

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(k_ghost_cell_x[index]), prv_cell_centre_y(OPEN_ROW_Y)));
    }
}

/* --- the HUD (FR-024/025) ------------------------------------------------ */

void test_the_hud_is_drawn_below_the_maze(void)
{
    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_GREATER_THAN_UINT16(0U, prv_count_hud_ink());
}

void test_the_hud_follows_the_score(void)
{
    uint16_t ink_at_zero;

    game_view_draw(&g_framebuffer, &g_snapshot);
    ink_at_zero = prv_count_hud_ink();

    g_snapshot.score = 123456U;
    game_view_draw(&g_framebuffer, &g_snapshot);

    TEST_ASSERT_NOT_EQUAL_UINT16(ink_at_zero, prv_count_hud_ink());
}

void test_the_hud_follows_the_lives(void)
{
    uint16_t ink_with_three_lives;

    game_view_draw(&g_framebuffer, &g_snapshot);
    ink_with_three_lives = prv_count_hud_ink();

    g_snapshot.lives = 1U;
    game_view_draw(&g_framebuffer, &g_snapshot);

    /* One pip per life (FR-024): fewer lives, less ink. */
    TEST_ASSERT_LESS_THAN_UINT16(ink_with_three_lives, prv_count_hud_ink());
}

/* --- every level draws --------------------------------------------------- */

void test_all_five_mazes_can_be_drawn(void)
{
    for (uint8_t level = PLAYFIELD_FIRST_LEVEL; level <= PLAYFIELD_LEVEL_COUNT; ++level)
    {
        g_snapshot.level = level;
        g_snapshot.pacman_cell.x = 5;
        g_snapshot.pacman_cell.y = 7;

        game_view_draw(&g_framebuffer, &g_snapshot);

        /* The outer wall is common to all five, so it is the one thing that must appear
         * whichever maze was loaded. */
        TEST_ASSERT_TRUE(prv_is_ink(prv_cell_centre_x(0), prv_cell_centre_y(0)));
    }
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_framebuffer_asserts(void)
{
    ASSERT_PROBE_EXPECT(game_view_draw(NULL, &g_snapshot), "inout_framebuffer != NULL");
}

void test_a_null_snapshot_asserts(void)
{
    ASSERT_PROBE_EXPECT(game_view_draw(&g_framebuffer, NULL), "in_snapshot != NULL");
}
