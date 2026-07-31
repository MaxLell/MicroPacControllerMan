/*
 * test_game_view.c
 *
 * The layer that turns cells into pixels.
 *
 * Everything here is a decision that would otherwise only be checkable by looking at a
 * panel: where the maze sits, how far along a step an actor is drawn, which ghost is
 * blue, and — the one with teeth — that a pellet disappearing and the actor that ate it
 * reach the screen in the *same* frame.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "game_view.h"
#include "msg.h"
#include "playfield.h"
#include "sprite_set.h"

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define LEVEL_1     (1U)
#define OPEN_COLUMN (1U) /* row 3 of the level-1 maze is "#.........#" */
#define OPEN_ROW    (3U)

static game_view_t g_view;
static msg_game_state_t g_state;

/* A state with the level-1 field and everyone parked on an open cell. */
static void prv_make_state(void)
{
    playfield_t maze;

    memset(&g_state, 0, sizeof(g_state));
    g_state.level = LEVEL_1;
    g_state.lives = 3U;

    playfield_load_level(&maze, LEVEL_1);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            const cell_t cell = {(int16_t)column, (int16_t)row};
            const playfield_pellet_e pellet = playfield_get_pellet(&maze, cell);

            msg_cell_bitmap_set(g_state.has_pellet, column, row, pellet != PLAYFIELD_PELLET_NONE);
            msg_cell_bitmap_set(g_state.is_power, column, row, pellet == PLAYFIELD_PELLET_POWER);
        }
    }

    g_state.pacman.column = OPEN_COLUMN;
    g_state.pacman.row = OPEN_ROW;
    g_state.pacman.direction = (uint8_t)DIRECTION_EAST;

    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        g_state.ghosts[index].column = (uint8_t)(4U + index);
        g_state.ghosts[index].row = 4U;
        g_state.ghosts[index].direction = (uint8_t)DIRECTION_EAST;
    }
}

/* Drain the level-change handover so a test can look at an ordinary frame. */
static msg_display_list_t prv_settle(void)
{
    msg_display_list_t list;

    while (game_view_is_field_pending(&g_view))
    {
        (void)game_view_get_display_list(&g_view, &list);
    }

    (void)game_view_get_display_list(&g_view, &list);

    return list;
}

static const msg_display_item_t* prv_find_actor(const msg_display_list_t* in_list, uint8_t in_sprite)
{
    for (uint8_t index = 0U; index < in_list->count; ++index)
    {
        if ((in_list->items[index].kind == DISPLAY_ITEM_ACTOR) && (in_list->items[index].sprite == in_sprite))
        {
            return &in_list->items[index];
        }
    }

    return NULL;
}

static uint8_t prv_count_of_kind(const msg_display_list_t* in_list, display_item_kind_e in_kind)
{
    uint8_t count = 0U;

    for (uint8_t index = 0U; index < in_list->count; ++index)
    {
        count += (in_list->items[index].kind == (uint8_t)in_kind) ? 1U : 0U;
    }

    return count;
}

void setUp(void)
{
    assert_probe_begin();
    game_view_init(&g_view);
    prv_make_state();
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * layout
 * ========================================================================= */

void test_the_maze_is_centred_across_the_panel(void)
{
    int16_t left_x;
    int16_t right_x;
    int16_t y;

    game_view_get_cell_pixel(0U, 0U, &left_x, &y);
    game_view_get_cell_pixel((uint8_t)(PLAYFIELD_WIDTH - 1U), 0U, &right_x, &y);

    /* 11 tiles of 20 px is 220 across a 240 px panel: 10 px either side. */
    TEST_ASSERT_EQUAL_INT16(10, left_x);
    TEST_ASSERT_EQUAL_INT16(10, FRAMEBUFFER_WIDTH - (right_x + GAME_VIEW_TILE_SIZE));
}

void test_the_maze_leaves_room_below_it(void)
{
    int16_t x;
    int16_t bottom_y;

    game_view_get_cell_pixel(0U, (uint8_t)(PLAYFIELD_HEIGHT - 1U), &x, &bottom_y);

    /* Whatever the score and lives end up looking like, they need somewhere to go. */
    TEST_ASSERT_GREATER_THAN_INT16(100, FRAMEBUFFER_HEIGHT - (bottom_y + GAME_VIEW_TILE_SIZE));
}

/* ==========================================================================
 * interpolation (10 §10.1)
 * ========================================================================= */

void test_an_actor_on_its_cell_is_drawn_on_the_tile(void)
{
    const msg_display_item_t* pacman;
    msg_display_list_t list;
    int16_t x;
    int16_t y;

    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    pacman = prv_find_actor(&list, (uint8_t)SPRITE_SET_PACMAN_CLOSED);
    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &x, &y);

    TEST_ASSERT_NOT_NULL(pacman);
    TEST_ASSERT_EQUAL_INT16(x, pacman->x);
    TEST_ASSERT_EQUAL_INT16(y, pacman->y);
}

void test_a_step_in_progress_moves_the_actor_between_two_cells(void)
{
    msg_display_list_t list;
    int16_t cell_x;
    int16_t cell_y;
    const msg_display_item_t* pacman;

    g_state.pacman.progress = 128U; /* half way */
    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    pacman = prv_find_actor(&list, (uint8_t)SPRITE_SET_PACMAN_OPEN_EAST);
    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &cell_x, &cell_y);

    /* Half a tile east, and not a pixel down: this is the whole reason 60 FPS was worth
     * asking for. */
    TEST_ASSERT_NOT_NULL(pacman);
    TEST_ASSERT_EQUAL_INT16(cell_x + (GAME_VIEW_TILE_SIZE / 2), pacman->x);
    TEST_ASSERT_EQUAL_INT16(cell_y, pacman->y);
}

void test_each_direction_interpolates_the_right_way(void)
{
    static const struct
    {
        direction_e direction;
        int16_t delta_x;
        int16_t delta_y;
    } cases[] = {
        {DIRECTION_NORTH, 0, -(GAME_VIEW_TILE_SIZE / 2)},
        {DIRECTION_SOUTH, 0, GAME_VIEW_TILE_SIZE / 2},
        {DIRECTION_WEST, -(GAME_VIEW_TILE_SIZE / 2), 0},
        {DIRECTION_EAST, GAME_VIEW_TILE_SIZE / 2, 0},
    };

    int16_t cell_x;
    int16_t cell_y;

    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &cell_x, &cell_y);

    for (uint8_t index = 0U; index < (sizeof(cases) / sizeof(cases[0])); ++index)
    {
        msg_display_list_t list;
        const msg_display_item_t* ghost;

        game_view_init(&g_view);
        g_state.ghosts[0].column = OPEN_COLUMN;
        g_state.ghosts[0].row = OPEN_ROW;
        g_state.ghosts[0].direction = (uint8_t)cases[index].direction;
        g_state.ghosts[0].progress = 128U;

        game_view_set_state(&g_view, &g_state);
        list = prv_settle();

        ghost = prv_find_actor(&list, (uint8_t)sprite_set_get_ghost_sprite(cases[index].direction));

        TEST_ASSERT_NOT_NULL(ghost);
        TEST_ASSERT_EQUAL_INT16(cell_x + cases[index].delta_x, ghost->x);
        TEST_ASSERT_EQUAL_INT16(cell_y + cases[index].delta_y, ghost->y);
    }
}

/* ==========================================================================
 * who is drawn, and in what
 * ========================================================================= */

void test_every_actor_reaches_the_frame(void)
{
    msg_display_list_t list;

    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    TEST_ASSERT_EQUAL_UINT8(MSG_ACTOR_COUNT, prv_count_of_kind(&list, DISPLAY_ITEM_ACTOR));
}

void test_the_four_ghosts_get_their_own_colours(void)
{
    msg_display_list_t list;
    uint8_t palettes[MSG_GHOST_COUNT];
    uint8_t found = 0U;

    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    for (uint8_t index = 0U; index < list.count; ++index)
    {
        if ((list.items[index].kind == DISPLAY_ITEM_ACTOR)
            && (list.items[index].palette != (uint8_t)SPRITE_SET_PALETTE_PACMAN))
        {
            palettes[found] = list.items[index].palette;
            ++found;
        }
    }

    TEST_ASSERT_EQUAL_UINT8(MSG_GHOST_COUNT, found);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_PALETTE_BLINKY, palettes[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_PALETTE_CLYDE, palettes[MSG_GHOST_COUNT - 1U]);
}

void test_a_frightened_ghost_turns_blue_and_the_others_do_not(void)
{
    msg_display_list_t list;
    uint8_t frightened_count = 0U;

    g_state.frightened_ghosts = 0x05U; /* Blinky and Inky */
    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    for (uint8_t index = 0U; index < list.count; ++index)
    {
        if (list.items[index].palette == (uint8_t)SPRITE_SET_PALETTE_FRIGHTENED)
        {
            ++frightened_count;
            TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_GHOST_FRIGHTENED, list.items[index].sprite);
        }
    }

    TEST_ASSERT_EQUAL_UINT8(2U, frightened_count);
}

void test_pacman_is_drawn_over_a_ghost_on_the_same_cell(void)
{
    msg_display_list_t list;
    uint8_t pacman_index = 0U;
    uint8_t last_ghost_index = 0U;

    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    for (uint8_t index = 0U; index < list.count; ++index)
    {
        if (list.items[index].palette == (uint8_t)SPRITE_SET_PALETTE_PACMAN)
        {
            pacman_index = index;
        }
        else if (list.items[index].kind == DISPLAY_ITEM_ACTOR)
        {
            last_ghost_index = index;
        }
        else
        {
            /* A field tile. */
        }
    }

    /* Later in the list is later on the screen. The moment a ghost and Pacman meet is
     * the one the player is watching. */
    TEST_ASSERT_GREATER_THAN_UINT8(last_ghost_index, pacman_index);
}

/* ==========================================================================
 * the field
 * ========================================================================= */

void test_a_new_level_hands_over_every_cell(void)
{
    msg_display_list_t list;
    uint16_t tile_count = 0U;

    game_view_set_state(&g_view, &g_state);

    while (game_view_is_field_pending(&g_view))
    {
        (void)game_view_get_display_list(&g_view, &list);
        tile_count += prv_count_of_kind(&list, DISPLAY_ITEM_BACKGROUND);
    }

    TEST_ASSERT_EQUAL_UINT16(PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT, tile_count);
}

void test_an_ordinary_frame_carries_no_field_at_all(void)
{
    const msg_display_list_t list = (game_view_set_state(&g_view, &g_state), prv_settle());

    TEST_ASSERT_EQUAL_UINT8(0U, prv_count_of_kind(&list, DISPLAY_ITEM_BACKGROUND));
}

void test_an_eaten_pellet_and_the_actors_arrive_in_the_same_frame(void)
{
    msg_display_list_t list;

    game_view_set_state(&g_view, &g_state);
    (void)prv_settle();

    /* Pacman swallows the one he is standing on. */
    msg_cell_bitmap_set(g_state.has_pellet, OPEN_COLUMN, OPEN_ROW, false);
    game_view_set_state(&g_view, &g_state);
    (void)game_view_get_display_list(&g_view, &list);

    /* Both, in one message. Split across two frames, the dot would linger for a frame
     * after it was eaten or vanish a frame early — and either reads as a glitch. */
    TEST_ASSERT_EQUAL_UINT8(1U, prv_count_of_kind(&list, DISPLAY_ITEM_BACKGROUND));
    TEST_ASSERT_EQUAL_UINT8(MSG_ACTOR_COUNT, prv_count_of_kind(&list, DISPLAY_ITEM_ACTOR));
}

void test_an_eaten_pellet_is_reported_once(void)
{
    msg_display_list_t list;

    game_view_set_state(&g_view, &g_state);
    (void)prv_settle();

    msg_cell_bitmap_set(g_state.has_pellet, OPEN_COLUMN, OPEN_ROW, false);
    game_view_set_state(&g_view, &g_state);
    (void)game_view_get_display_list(&g_view, &list);

    game_view_set_state(&g_view, &g_state);
    (void)game_view_get_display_list(&g_view, &list);

    TEST_ASSERT_EQUAL_UINT8(0U, prv_count_of_kind(&list, DISPLAY_ITEM_BACKGROUND));
}

void test_the_emptied_cell_is_drawn_as_empty(void)
{
    msg_display_list_t list;
    int16_t x;
    int16_t y;

    game_view_set_state(&g_view, &g_state);
    (void)prv_settle();

    msg_cell_bitmap_set(g_state.has_pellet, OPEN_COLUMN, OPEN_ROW, false);
    game_view_set_state(&g_view, &g_state);
    (void)game_view_get_display_list(&g_view, &list);

    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &x, &y);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)DISPLAY_ITEM_BACKGROUND, list.items[0].kind);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_TILE, list.items[0].sprite);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_PALETTE_EMPTY, list.items[0].palette);
    TEST_ASSERT_EQUAL_INT16(x, list.items[0].x);
    TEST_ASSERT_EQUAL_INT16(y, list.items[0].y);
}

void test_a_wall_is_a_wall_and_a_power_pellet_is_bigger(void)
{
    msg_display_list_t list;
    bool has_wall = false;
    bool has_power = false;

    game_view_set_state(&g_view, &g_state);

    while (game_view_is_field_pending(&g_view))
    {
        (void)game_view_get_display_list(&g_view, &list);

        for (uint8_t index = 0U; index < list.count; ++index)
        {
            has_wall = has_wall || (list.items[index].palette == (uint8_t)SPRITE_SET_PALETTE_WALL);
            has_power = has_power || (list.items[index].sprite == (uint8_t)SPRITE_SET_TILE_POWER_PELLET);
        }
    }

    TEST_ASSERT_TRUE(has_wall);
    TEST_ASSERT_TRUE(has_power);
}

void test_nothing_is_drawn_before_a_state_arrives(void)
{
    msg_display_list_t list;

    TEST_ASSERT_FALSE(game_view_get_display_list(&g_view, &list));
    TEST_ASSERT_EQUAL_UINT8(0U, list.count);
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_null_arguments_assert(void)
{
    msg_display_list_t list;
    int16_t coordinate;

    ASSERT_PROBE_EXPECT(game_view_init(NULL), "inout_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_state(NULL, &g_state), "inout_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_state(&g_view, NULL), "in_state != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_get_display_list(NULL, &list), "inout_view != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_get_display_list(&g_view, NULL), "out_list != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_is_field_pending(NULL), "in_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_get_cell_pixel(0U, 0U, NULL, &coordinate), "out_x != NULL");
    ASSERT_PROBE_EXPECT(game_view_get_cell_pixel(0U, 0U, &coordinate, NULL), "out_y != NULL");
}
