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
#include <stdio.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "game_view.h"
#include "maze_gen.h"
#include "msg.h"
#include "playfield.h"
#include "sprite_set.h"

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define LEVEL_1               (1U)
#define GAME_VIEW_ACTOR_INSET ((GAME_VIEW_ACTOR_SIZE - GAME_VIEW_TILE_SIZE) / 2)
/* The bottom corridor of the arcade maze is open all the way across. */
#define OPEN_COLUMN           (15U)
#define OPEN_ROW              (29U)

/* A cell that really does hold a pellet. Pacman's own start cell does not — which is
 * what the first version of the eaten-pellet tests below tripped over, passing for the
 * wrong reason until the maze changed and it started failing honestly. */
#define PELLET_COLUMN         (16U)
#define PELLET_ROW            (29U)

static game_view_t g_view;
static msg_game_state_t g_state;

/* A state with the level-1 field and everyone parked on an open cell. */
static void prv_make_state(void)
{
    playfield_t maze;

    memset(&g_state, 0, sizeof(g_state));
    g_state.level = LEVEL_1;
    g_state.lives = 3U;

    playfield_load(&maze);

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

    /* Arrived, not mid-step: `progress` counts how far *into* the cell an actor has come,
     * so a zero here would mean "still a whole cell back the way it came" and every
     * position these tests check would be off by a tile. */
    g_state.pacman.column = OPEN_COLUMN;
    g_state.pacman.row = OPEN_ROW;
    g_state.pacman.direction = (uint8_t)DIRECTION_EAST;
    g_state.pacman.progress = MSG_CELL_PROGRESS_ARRIVED;

    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        g_state.ghosts[index].column = (uint8_t)(4U + index);
        g_state.ghosts[index].row = 4U;
        g_state.ghosts[index].direction = (uint8_t)DIRECTION_EAST;
        g_state.ghosts[index].progress = MSG_CELL_PROGRESS_ARRIVED;
    }
}

/* Drain the level-change handover *and* the HUD, so a test can look at an ordinary frame.
 *
 * Both trickle: the field over as many messages as it takes, then the HUD a few slots at a
 * time beside the actors. What is left once neither has anything more to say is a frame
 * carrying actors only, which is what most of the tests below want to start from. */
static msg_display_list_t prv_settle(void)
{
    /* Generous, and bounded only so a view that never converges fails as a test rather
     * than hanging the suite. */
    const uint16_t give_up_after = 2000U;
    msg_display_list_t list;
    uint16_t frames = 0U;

    while (game_view_is_field_pending(&g_view))
    {
        (void)game_view_get_display_list(&g_view, &list);

        TEST_ASSERT_LESS_THAN_UINT16(give_up_after, ++frames);
    }

    do
    {
        uint8_t background = 0U;

        (void)game_view_get_display_list(&g_view, &list);

        for (uint8_t index = 0U; index < list.count; ++index)
        {
            background += (list.items[index].kind == (uint8_t)DISPLAY_ITEM_BACKGROUND) ? 1U : 0U;
        }

        TEST_ASSERT_LESS_THAN_UINT16(give_up_after, ++frames);

        if (background == 0U)
        {
            break;
        }
    } while (true);

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

/* The maze the view is asked to draw. The arcade's own throughout this file: the appearance
 * is *derived* from the walls now (FR-029), and the arcade layout is the one case where the
 * answer is written down independently — in the map at the bottom of this file, transcribed
 * from the 1980 game — so it is what the derivation is judged against. */
static playfield_t g_arcade_rules;

static void prv_set_arcade_maze(void)
{
    playfield_map_t map;

    playfield_get_arcade_map(&map);
    playfield_load_from_map(&g_arcade_rules, &map);
    game_view_set_maze(&g_view, &map);
}

void setUp(void)
{
    assert_probe_begin();
    game_view_init(&g_view);
    prv_set_arcade_maze();
    prv_make_state();
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * the HUD
 * ========================================================================= */

/* Collect every background item of the next few frames, so the HUD can be inspected as a
 * whole even though it goes out a few slots at a time. */
static uint8_t prv_collect_hud(msg_display_item_t* out_items, uint8_t in_capacity)
{
    msg_display_list_t list;
    uint8_t count = 0U;

    for (uint8_t frame = 0U; frame < 40U; ++frame)
    {
        (void)game_view_get_display_list(&g_view, &list);

        for (uint8_t index = 0U; index < list.count; ++index)
        {
            if (list.items[index].kind != (uint8_t)DISPLAY_ITEM_BACKGROUND)
            {
                continue;
            }

            TEST_ASSERT_LESS_THAN_UINT8(in_capacity, count);
            out_items[count] = list.items[index];
            ++count;
        }
    }

    return count;
}

/* The item drawn at a pixel position, or NULL. */
static const msg_display_item_t* prv_find_at(const msg_display_item_t* in_items, uint8_t in_count, int16_t in_x,
                                             int16_t in_y)
{
    for (uint8_t index = 0U; index < in_count; ++index)
    {
        if ((in_items[index].x == in_x) && (in_items[index].y == in_y))
        {
            return &in_items[index];
        }
    }

    return NULL;
}

/* Where the score's digit at `in_place` lands, counting places from the units up. */
static void prv_get_score_digit_pixel(uint8_t in_place, int16_t* const out_x, int16_t* const out_y)
{
    game_view_get_cell_pixel((uint8_t)(6U - in_place), 0U, out_x, out_y);

    *out_y = GAME_VIEW_HUD_VALUE_ROW_Y;
}

void test_the_hud_spells_out_the_score_the_level_and_the_lives(void)
{
    msg_display_item_t items[GAME_VIEW_HUD_ITEM_COUNT * 2U];
    uint8_t count;
    int16_t x;
    int16_t y;

    g_state.score = 1234U;
    g_state.level = 7U;
    g_state.lives = 3U;

    game_view_set_state(&g_view, &g_state);
    while (game_view_is_field_pending(&g_view))
    {
        msg_display_list_t list;
        (void)game_view_get_display_list(&g_view, &list);
    }

    count = prv_collect_hud(items, (uint8_t)(sizeof(items) / sizeof(items[0])));

    /* The units digit, and the one above it. Right-aligned, so 1234 puts the 4 on the last
     * column and the 3 beside it. */
    prv_get_score_digit_pixel(0U, &x, &y);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph('4'), prv_find_at(items, count, x, y)->sprite);
    prv_get_score_digit_pixel(1U, &x, &y);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph('3'), prv_find_at(items, count, x, y)->sprite);

    /* And the places above the number are blank rather than zeroes — the arcade does not
     * pad a score out with leading noughts. */
    prv_get_score_digit_pixel(4U, &x, &y);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph(' '), prv_find_at(items, count, x, y)->sprite);

    /* The level, right-aligned in its own two places at the other end. */
    game_view_get_cell_pixel(26U, 0U, &x, &y);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph('7'),
                            prv_find_at(items, count, x, GAME_VIEW_HUD_VALUE_ROW_Y)->sprite);

    /* Three lives, three little Pacmans along the bottom. */
    for (uint8_t slot = 0U; slot < 3U; ++slot)
    {
        game_view_get_cell_pixel((uint8_t)(2U + (slot * 2U)), 0U, &x, &y);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_PACMAN_HALF_WEST,
                                prv_find_at(items, count, x, GAME_VIEW_HUD_LIVES_Y)->sprite);
    }
}

void test_a_score_of_nothing_still_shows_a_nought(void)
{
    msg_display_item_t items[GAME_VIEW_HUD_ITEM_COUNT * 2U];
    uint8_t count;
    int16_t x;
    int16_t y;

    g_state.score = 0U;
    game_view_set_state(&g_view, &g_state);
    while (game_view_is_field_pending(&g_view))
    {
        msg_display_list_t list;
        (void)game_view_get_display_list(&g_view, &list);
    }

    count = prv_collect_hud(items, (uint8_t)(sizeof(items) / sizeof(items[0])));

    /* Blanking every leading zero would leave the row empty at the start of a run, which
     * reads as a HUD that has not come up yet. */
    prv_get_score_digit_pixel(0U, &x, &y);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph('0'), prv_find_at(items, count, x, y)->sprite);
}

void test_only_the_digits_that_moved_are_sent_again(void)
{
    msg_display_item_t items[GAME_VIEW_HUD_ITEM_COUNT * 2U];
    uint8_t count;

    g_state.score = 1230U;
    game_view_set_state(&g_view, &g_state);
    (void)prv_settle();

    /* Ten points. One digit moves, and the score is the thing that changes most often in
     * the whole game — re-sending all seven every pellet would cost more of the frame than
     * the five actors do. */
    g_state.score = 1240U;
    game_view_set_state(&g_view, &g_state);
    count = prv_collect_hud(items, (uint8_t)(sizeof(items) / sizeof(items[0])));

    TEST_ASSERT_EQUAL_UINT8(1U, count);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_glyph('4'), items[0].sprite);
}

void test_a_lost_life_is_wiped_rather_than_left_behind(void)
{
    msg_display_item_t items[GAME_VIEW_HUD_ITEM_COUNT * 2U];
    uint8_t count;
    int16_t x;
    int16_t y;

    g_state.lives = 3U;
    game_view_set_state(&g_view, &g_state);
    (void)prv_settle();

    g_state.lives = 2U;
    game_view_set_state(&g_view, &g_state);
    count = prv_collect_hud(items, (uint8_t)(sizeof(items) / sizeof(items[0])));

    /* The slot has to be painted over. Simply not drawing it again would leave the third
     * Pacman on the panel for the rest of the run, because nothing else ever covers it. */
    game_view_get_cell_pixel(6U, 0U, &x, &y);

    TEST_ASSERT_EQUAL_UINT8(1U, count);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SPRITE_SET_ACTOR_BLANK,
                            prv_find_at(items, count, x, GAME_VIEW_HUD_LIVES_Y)->sprite);
}

void test_the_hud_never_lands_on_the_maze(void)
{
    /* The maze owns rows 0..30 of its own grid and the HUD lives in what is left above and
     * below it. Overlap would be silently destructive: a HUD item would paint over a wall
     * and nothing would ever put it back, because walls are only drawn on a level change. */
    msg_display_item_t items[GAME_VIEW_HUD_ITEM_COUNT * 2U];
    uint8_t count;
    int16_t maze_top;
    int16_t maze_left;
    const int16_t maze_bottom = (int16_t)(GAME_VIEW_ORIGIN_Y + (PLAYFIELD_HEIGHT * GAME_VIEW_TILE_SIZE));

    game_view_get_cell_pixel(0U, 0U, &maze_left, &maze_top);

    game_view_set_state(&g_view, &g_state);
    while (game_view_is_field_pending(&g_view))
    {
        msg_display_list_t list;
        (void)game_view_get_display_list(&g_view, &list);
    }

    count = prv_collect_hud(items, (uint8_t)(sizeof(items) / sizeof(items[0])));

    TEST_ASSERT_GREATER_THAN_UINT8(0U, count);

    for (uint8_t index = 0U; index < count; ++index)
    {
        const int16_t bottom = (int16_t)(items[index].y + GAME_VIEW_ACTOR_SIZE);
        char message[72];

        (void)snprintf(message, sizeof(message), "a HUD item at %d,%d overlaps the maze", items[index].x,
                       items[index].y);
        TEST_ASSERT_TRUE_MESSAGE((bottom <= maze_top) || (items[index].y >= maze_bottom), message);

        (void)snprintf(message, sizeof(message), "a HUD item at %d,%d runs off the panel", items[index].x,
                       items[index].y);
        TEST_ASSERT_TRUE_MESSAGE((items[index].y >= 0) && (bottom <= FRAMEBUFFER_HEIGHT), message);
    }
}

/* ==========================================================================
 * the picture and the rules must agree
 * ========================================================================= */

void test_the_drawn_maze_and_the_played_maze_agree(void)
{
    /* `playfield` says where a wall *is* and this module says what a wall *looks like*. The
     * appearance is derived from the walls now (FR-029), so the two cannot disagree about
     * *where* — but they can still disagree about what counts as passable, which is what this
     * checks. It used to guard a hand-written second copy of the maze;
     * and a corner piece does not say whether a ghost may stand there. So the maze is
     * written out twice, from the same source, and this is what stops the two drifting.
     *
     * Every cell drawn as wall art has to be a wall in the rules, and every cell the rules
     * let an entity stand on has to be drawn as something walkable. Get it wrong and you
     * get the two worst bugs this pairing can have: an invisible wall, or a wall Pacman
     * walks straight through. */
    playfield_t maze;

    playfield_load(&maze);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            const cell_t cell = {(int16_t)column, (int16_t)row};
            const bool is_walkable = playfield_is_walkable(&maze, cell);
            const bool is_drawn_as_wall = game_view_is_wall_drawn_at(&g_view, column, row);
            char message[72];

            (void)snprintf(message, sizeof(message), "cell %u,%u: drawn as wall %u, walkable %u", column, row,
                           (unsigned)is_drawn_as_wall, (unsigned)is_walkable);
            TEST_ASSERT_FALSE_MESSAGE(is_drawn_as_wall && is_walkable, message);
        }
    }
}

void test_the_ghost_house_gate_is_drawn_but_not_a_wall(void)
{
    /* The one place the two maps disagree on purpose: the gate is drawn — the arcade puts a
     * pink bar there — but the rules let a ghost through it, which is how they get out. */
    playfield_t maze;
    const cell_t gate = {13, 12};

    playfield_load(&maze);

    TEST_ASSERT_TRUE(playfield_is_walkable(&maze, gate));
    TEST_ASSERT_FALSE(game_view_is_wall_drawn_at(&g_view, 13U, 12U));
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

    /* 28 tiles of 8 px is 224 across a 240 px panel: 8 px either side, and the same
     * margin on both — derived rather than typed, so the assertion survives a change of
     * tile size and still fails on a lopsided maze. */
    const int16_t margin = (int16_t)((FRAMEBUFFER_WIDTH - (PLAYFIELD_WIDTH * GAME_VIEW_TILE_SIZE)) / 2);

    TEST_ASSERT_EQUAL_INT16(margin, left_x);
    TEST_ASSERT_EQUAL_INT16(margin, FRAMEBUFFER_WIDTH - (right_x + GAME_VIEW_TILE_SIZE));
}

void test_the_maze_leaves_room_below_it(void)
{
    int16_t x;
    int16_t bottom_y;

    game_view_get_cell_pixel(0U, (uint8_t)(PLAYFIELD_HEIGHT - 1U), &x, &bottom_y);

    /* Rows above and below the maze, where the arcade puts the score and the lives.
     * Three cells above and at least three below is what 31 rows of 8 px leave on a
     * 320 px panel. */
    TEST_ASSERT_GREATER_OR_EQUAL_INT16(3 * GAME_VIEW_TILE_SIZE, GAME_VIEW_ORIGIN_Y);
    TEST_ASSERT_GREATER_OR_EQUAL_INT16(3 * GAME_VIEW_TILE_SIZE, FRAMEBUFFER_HEIGHT - (bottom_y + GAME_VIEW_TILE_SIZE));
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

    pacman = prv_find_actor(&list, (uint8_t)sprite_set_get_pacman_sprite(DIRECTION_EAST, g_state.pacman.progress));
    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &x, &y);

    /* Centred on its cell rather than boxed into it: a 16 px sprite on an 8 px cell hangs
     * half a cell out on every side, which is how the arcade fills a corridor. */
    TEST_ASSERT_NOT_NULL(pacman);
    TEST_ASSERT_EQUAL_INT16(x - ((GAME_VIEW_ACTOR_SIZE - GAME_VIEW_TILE_SIZE) / 2), pacman->x);
    TEST_ASSERT_EQUAL_INT16(y - ((GAME_VIEW_ACTOR_SIZE - GAME_VIEW_TILE_SIZE) / 2), pacman->y);
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

    pacman = prv_find_actor(&list, (uint8_t)sprite_set_get_pacman_sprite(DIRECTION_EAST, g_state.pacman.progress));
    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &cell_x, &cell_y);

    /* Half a tile *west* of the cell, and not a pixel up or down. He is heading east and
     * is half way through the step that lands him here, so he is still behind it — the
     * interpolation runs from the cell he came from into the one he is on. This is the
     * whole reason 60 FPS was worth asking for. */
    TEST_ASSERT_NOT_NULL(pacman);
    TEST_ASSERT_EQUAL_INT16(cell_x - (GAME_VIEW_TILE_SIZE / 2) - GAME_VIEW_ACTOR_INSET, pacman->x);
    TEST_ASSERT_EQUAL_INT16(cell_y - GAME_VIEW_ACTOR_INSET, pacman->y);
}

void test_each_direction_interpolates_the_right_way(void)
{
    static const struct
    {
        direction_e direction;
        int16_t delta_x;
        int16_t delta_y;
    } cases[] = {
        /* Half a step *behind* the cell, on the side it came from — an actor heading north
         * is still below the cell it is arriving at. */
        {DIRECTION_NORTH, 0, GAME_VIEW_TILE_SIZE / 2},
        {DIRECTION_SOUTH, 0, -(GAME_VIEW_TILE_SIZE / 2)},
        {DIRECTION_WEST, GAME_VIEW_TILE_SIZE / 2, 0},
        {DIRECTION_EAST, -(GAME_VIEW_TILE_SIZE / 2), 0},
    };

    int16_t cell_x;
    int16_t cell_y;

    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &cell_x, &cell_y);

    for (uint8_t index = 0U; index < (sizeof(cases) / sizeof(cases[0])); ++index)
    {
        msg_display_list_t list;
        const msg_display_item_t* ghost;

        game_view_init(&g_view);
        prv_set_arcade_maze();
        g_state.ghosts[0].column = OPEN_COLUMN;
        g_state.ghosts[0].row = OPEN_ROW;
        g_state.ghosts[0].direction = (uint8_t)cases[index].direction;
        g_state.ghosts[0].progress = 128U;

        game_view_set_state(&g_view, &g_state);
        list = prv_settle();

        ghost = prv_find_actor(
            &list, (uint8_t)sprite_set_get_ghost_sprite(cases[index].direction, g_state.ghosts[0].progress));

        TEST_ASSERT_NOT_NULL(ghost);
        TEST_ASSERT_EQUAL_INT16(cell_x + cases[index].delta_x - GAME_VIEW_ACTOR_INSET, ghost->x);
        TEST_ASSERT_EQUAL_INT16(cell_y + cases[index].delta_y - GAME_VIEW_ACTOR_INSET, ghost->y);
    }
}

/* Where the actor with this sprite is drawn, for the state as it stands. */
static void prv_get_drawn_actor_pixel(uint8_t in_sprite, int16_t* const out_x, int16_t* const out_y)
{
    msg_display_list_t list;
    const msg_display_item_t* actor;

    game_view_set_state(&g_view, &g_state);
    list = prv_settle();

    actor = prv_find_actor(&list, in_sprite);

    TEST_ASSERT_NOT_NULL(actor);

    *out_x = actor->x;
    *out_y = actor->y;
}

void test_a_corner_is_drawn_without_a_jump(void)
{
    /* The regression this whole scheme exists for, and it was very visible: rounding a
     * corner, Pacman appeared to stall for a moment and then jump.
     *
     * The old interpolation ran *forward* from the cell along the current facing, towards
     * a cell that is only chosen when the next step happens. Coming east into a corner it
     * therefore either slid a full cell further east and snapped back north, or — with a
     * wall to the east, which is what makes it a corner — was pinned in place for a whole
     * period and then jumped a cell. Both are one bug: the destination was a guess.
     *
     * Here the two frames either side of the turn are asked for directly. Last frame of
     * the step east: arrived on the corner cell. First frame of the step north: nothing of
     * the new step run off yet, so still on the corner cell. **The same pixel** — the two
     * straight runs meet, and there is nothing to stall or jump. */
    int16_t last_east_x;
    int16_t last_east_y;
    int16_t first_north_x;
    int16_t first_north_y;
    int16_t corner_x;
    int16_t corner_y;

    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &corner_x, &corner_y);

    g_state.pacman.column = OPEN_COLUMN;
    g_state.pacman.row = OPEN_ROW;
    g_state.pacman.direction = (uint8_t)DIRECTION_EAST;
    g_state.pacman.progress = MSG_CELL_PROGRESS_ARRIVED;
    prv_get_drawn_actor_pixel((uint8_t)sprite_set_get_pacman_sprite(DIRECTION_EAST, g_state.pacman.progress),
                              &last_east_x, &last_east_y);

    /* The model has stepped north: it is on the cell above, none of that step run off. */
    game_view_init(&g_view);
    prv_set_arcade_maze();
    g_state.pacman.row = (uint8_t)(OPEN_ROW - 1U);
    g_state.pacman.direction = (uint8_t)DIRECTION_NORTH;
    g_state.pacman.progress = 0U;
    prv_get_drawn_actor_pixel((uint8_t)sprite_set_get_pacman_sprite(DIRECTION_NORTH, g_state.pacman.progress),
                              &first_north_x, &first_north_y);

    TEST_ASSERT_EQUAL_INT16(last_east_x, first_north_x);
    TEST_ASSERT_EQUAL_INT16(last_east_y, first_north_y);

    /* And that shared pixel is the corner cell itself, not some point beyond it. */
    TEST_ASSERT_EQUAL_INT16(corner_x - GAME_VIEW_ACTOR_INSET, last_east_x);
    TEST_ASSERT_EQUAL_INT16(corner_y - GAME_VIEW_ACTOR_INSET, last_east_y);
}

void test_an_actor_that_is_not_moving_sits_on_its_cell(void)
{
    /* Stopped against a wall, Pacman keeps his facing (§10.1) and his timer keeps running.
     * The game reports "arrived" rather than a running fraction, so he stays put instead of
     * being slid in from the cell behind him once per period. */
    int16_t x;
    int16_t y;
    int16_t cell_x;
    int16_t cell_y;

    game_view_get_cell_pixel(OPEN_COLUMN, OPEN_ROW, &cell_x, &cell_y);

    g_state.pacman.progress = MSG_CELL_PROGRESS_ARRIVED;
    prv_get_drawn_actor_pixel((uint8_t)sprite_set_get_pacman_sprite(DIRECTION_EAST, g_state.pacman.progress), &x, &y);

    TEST_ASSERT_EQUAL_INT16(cell_x - GAME_VIEW_ACTOR_INSET, x);
    TEST_ASSERT_EQUAL_INT16(cell_y - GAME_VIEW_ACTOR_INSET, y);
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
            TEST_ASSERT_EQUAL_UINT8((uint8_t)sprite_set_get_frightened_sprite(g_state.ghosts[0].progress),
                                    list.items[index].sprite);
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

    /* The maze **and the border drawn round it** — the outer wall's second cell, which is what
     * lets it carry the same stroke as every other wall (DEC-033). */
    TEST_ASSERT_EQUAL_UINT16(GAME_VIEW_FIELD_WIDTH * GAME_VIEW_FIELD_HEIGHT, tile_count);
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
    msg_cell_bitmap_set(g_state.has_pellet, PELLET_COLUMN, PELLET_ROW, false);
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

    msg_cell_bitmap_set(g_state.has_pellet, PELLET_COLUMN, PELLET_ROW, false);
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

    msg_cell_bitmap_set(g_state.has_pellet, PELLET_COLUMN, PELLET_ROW, false);
    game_view_set_state(&g_view, &g_state);
    (void)game_view_get_display_list(&g_view, &list);

    game_view_get_cell_pixel(PELLET_COLUMN, PELLET_ROW, &x, &y);

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
 * the appearance is derived from the walls
 * ========================================================================= */

/* How the arcade draws its own maze, one character per cell, transcribed from the 1980 game;
 * `sprite_set_get_maze_tile` says which drawing each letter is.
 *
 * This used to live in `game_view.c` and *be* the appearance. It is a **test fixture** now:
 * the module derives the appearance from where the walls are, and this is the independent
 * answer that derivation is checked against. A rule that reproduces 764 hand-drawn cells is a
 * rule; one checked only against itself is a restatement. */
/* clang-format off */
static const char* const g_arcade_appearance[PLAYFIELD_HEIGHT] = {
    "0UUUUUUUUUUUU45UUUUUUUUUUUU1",
    "L............rl............R",
    "L.ebbf.ebbbf.rl.ebbbf.ebbf.R",
    "LPr  l.r   l.rl.r   l.r  lPR",
    "L.guuh.guuuh.gh.guuuh.guuh.R",
    "L..........................R",
    "L.ebbf.ef.ebbbbbbf.ef.ebbf.R",
    "L.guuh.rl.guuyxuuh.rl.guuh.R",
    "L......rl....rl....rl......R",
    "2BBBBf.rzbbf rl ebbwl.eBBBB3",
    "     L.rxuuh gh guuyl.R     ",
    "     L.rl          rl.R     ",
    "     L.rl mjs--tjn rl.R     ",
    "UUUUUh.gh i      q gh.gUUUUU",
    "      .   i      q   .      ",
    "BBBBBf.ef i      q ef.eBBBBB",
    "     L.rl okkkkkkp rl.R     ",
    "     L.rl          rl.R     ",
    "     L.rl ebbbbbbf rl.R     ",
    "0UUUUh.gh guuyxuuh gh.gUUUU1",
    "L............rl............R",
    "L.ebbf.ebbbf.rl.ebbbf.ebbf.R",
    "L.guyl.guuuh.gh.guuuh.rxuh.R",
    "LP..rl.......  .......rl..PR",
    "6bf.rl.ef.ebbbbbbf.ef.rl.eb8",
    "7uh.gh.rl.guuyxuuh.rl.gh.gu9",
    "L......rl....rl....rl......R",
    "L.ebbbbwzbbf.rl.ebbwzbbbbf.R",
    "L.guuuuuuuuh.gh.guuuuuuuuh.R",
    "L..........................R",
    "2BBBBBBBBBBBBBBBBBBBBBBBBBB3",
};
/* clang-format on */

/* The two side masses beside the tunnel, rows 9..19 and the six columns at each edge.
 *
 * The one place the derivation cannot match, and the reason is worth writing down: there the
 * arcade wall is six cells thick and is drawn with the *boundary* family, as a detour of the
 * outer frame rather than as a block with an outline. No generated maze has such a mass —
 * `maze_gen` builds a one-cell frame with blocks inside it — so the rule is right for every
 * maze that will actually be played, and wrong for 64 cells of the fixture. Excluded
 * deliberately, and counted, so the exclusion cannot quietly grow. */
/* The maze's outer wall, which the derivation no longer draws the arcade's way at all: it is a
 * two-cell wall now, its second cell in the panel's margin, carrying the same 6-pixel stroke as
 * every other wall ([DEC-033](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)). The
 * arcade's frame is one cell and a 2-pixel line, so there is nothing here to compare — and the
 * comparison this replaced was hollow anyway: it matched tile *ids* whose art had already been
 * replaced. Counted, so the ring cannot quietly grow. */
#define OUTER_WALL_EXPECTED_CELLS (114U)

/* The ghost house, whose picture is stamped rather than derived and now wears the same 6-pixel
 * wall as the rest of the maze instead of the arcade's 2 ([DEC-033]). It used to agree with the
 * arcade here by accident — the stamp named the arcade's own tiles — which made this test look
 * like it was checking the house when it never was. Skipped and counted, honestly. */
#define HOUSE_FIRST_COLUMN        (10U)
#define HOUSE_LAST_COLUMN         (17U)
#define HOUSE_FIRST_ROW           (12U)
#define HOUSE_LAST_ROW            (16U)
#define HOUSE_EXPECTED_CELLS      (40U)

static bool prv_is_ghost_house(uint8_t in_column, uint8_t in_row)
{
    return (in_column >= HOUSE_FIRST_COLUMN) && (in_column <= HOUSE_LAST_COLUMN) && (in_row >= HOUSE_FIRST_ROW)
           && (in_row <= HOUSE_LAST_ROW);
}

#define MASS_FIRST_ROW             (9U)
#define MASS_LAST_ROW              (19U)
#define MASS_LEFT_COLUMNS          (6U)
#define MASS_EXPECTED_CELLS        (80U)

/* How many cells of the arcade maze carry the *second* ring — the inner half of the 6-pixel
 * stroke, which the arcade draws as nothing because its own stroke is 3 pixels wide
 * ([DEC-032](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)). Counted so that a change
 * to the rule cannot quietly start or stop drawing inside walls. */
#define SECOND_RING_EXPECTED_CELLS (10U)

static bool prv_is_outer_wall(uint8_t in_column, uint8_t in_row)
{
    return (in_column == 0U) || (in_column == (PLAYFIELD_WIDTH - 1U)) || (in_row == 0U)
           || (in_row == (PLAYFIELD_HEIGHT - 1U));
}

static bool prv_is_in_a_side_mass(uint8_t in_column, uint8_t in_row)
{
    if ((in_row < MASS_FIRST_ROW) || (in_row > MASS_LAST_ROW))
    {
        return false;
    }

    return (in_column < MASS_LEFT_COLUMNS) || (in_column >= (PLAYFIELD_WIDTH - MASS_LEFT_COLUMNS));
}

void test_the_derived_appearance_is_the_arcade_s_own(void)
{
    uint16_t excluded = 0U;
    uint16_t second_ring = 0U;
    uint16_t outer_wall = 0U;
    uint16_t house = 0U;

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            sprite_set_id_e expected;
            const bool is_wall_art = sprite_set_get_maze_tile(g_arcade_appearance[row][column], &expected);
            const uint8_t derived = game_view_get_maze_tile(&g_view, column, row);
            char message[96];

            if (prv_is_outer_wall(column, row))
            {
                ++outer_wall;

                continue;
            }

            if (prv_is_ghost_house(column, row))
            {
                ++house;

                continue;
            }

            if (prv_is_in_a_side_mass(column, row))
            {
                const bool agrees = is_wall_art ? (derived == (uint8_t)expected) : (derived == GAME_VIEW_NO_WALL_TILE);

                if (!agrees)
                {
                    ++excluded;
                }

                continue;
            }

            (void)snprintf(message, sizeof(message), "cell %u,%u: arcade draws %c", column, row,
                           g_arcade_appearance[row][column]);

            if (is_wall_art)
            {
                TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)expected, derived, message);
            }
            else if (derived != GAME_VIEW_NO_WALL_TILE)
            {
                /* The second ring: the inner half of a 6-pixel stroke, which the arcade has no
                 * equivalent of because its own stroke is 3 (DEC-032). Only ever *inside* a
                 * wall — never on a cell an actor can stand on, which is the part that would
                 * matter. */
                const cell_t cell = {(int16_t)column, (int16_t)row};

                TEST_ASSERT_FALSE_MESSAGE(playfield_is_walkable(&g_arcade_rules, cell), message);
                ++second_ring;
            }
            else
            {
                /* Both agree there is nothing here. */
            }
        }
    }

    /* If a change makes the derivation agree with the masses too, or disagree more widely,
     * this is the line that notices. */
    TEST_ASSERT_EQUAL_UINT16(MASS_EXPECTED_CELLS, excluded);

    /* And this one notices the second ring growing or vanishing. It is ink the arcade does not
     * have, so it is counted rather than asserted away. */
    TEST_ASSERT_EQUAL_UINT16(SECOND_RING_EXPECTED_CELLS, second_ring);
    TEST_ASSERT_EQUAL_UINT16(OUTER_WALL_EXPECTED_CELLS, outer_wall);
    TEST_ASSERT_EQUAL_UINT16(HOUSE_EXPECTED_CELLS, house);
}

void test_a_generated_maze_is_drawn_with_real_tiles(void)
{
    /* The derivation has to hold up on the mazes that are actually played, not only on the
     * fixture. Nothing here knows what the maze looks like, so what is checked is what must be
     * true of any of them: every wall cell that borders a corridor gets a drawing, and no
     * corridor cell does. A missing drawing is a hole in the maze wall on the panel. */
    playfield_map_t map;
    playfield_t rules;

    maze_gen_generate(&map, 12345U);
    playfield_load_from_map(&rules, &map);
    game_view_set_maze(&g_view, &map);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            const cell_t cell = {(int16_t)column, (int16_t)row};
            const bool is_wall = map.rows[row][column] == PLAYFIELD_MAP_WALL;
            const uint8_t derived = game_view_get_maze_tile(&g_view, column, row);
            bool touches_corridor = false;
            char message[80];

            (void)snprintf(message, sizeof(message), "cell %u,%u", column, row);

            if (!is_wall)
            {
                /* Corridor, or the inside of the ghost house, which the house tiles frame but
                 * do not fill. */
                TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(&rules, cell), message);

                continue;
            }

            for (int16_t offset_y = -1; offset_y <= 1; ++offset_y)
            {
                for (int16_t offset_x = -1; offset_x <= 1; ++offset_x)
                {
                    const int16_t x = (int16_t)(column + offset_x);
                    const int16_t y = (int16_t)(row + offset_y);

                    if ((x < 0) || (x >= PLAYFIELD_WIDTH) || (y < 0) || (y >= PLAYFIELD_HEIGHT))
                    {
                        continue;
                    }

                    if (map.rows[y][x] != PLAYFIELD_MAP_WALL)
                    {
                        touches_corridor = true;
                    }
                }
            }

            if (touches_corridor)
            {
                TEST_ASSERT_NOT_EQUAL_UINT8_MESSAGE(GAME_VIEW_NO_WALL_TILE, derived, message);
            }
        }
    }
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_null_arguments_assert(void)
{
    msg_display_list_t list;
    playfield_map_t map;
    int16_t coordinate;

    playfield_get_arcade_map(&map);

    ASSERT_PROBE_EXPECT(game_view_init(NULL), "inout_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_state(NULL, &g_state), "inout_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_state(&g_view, NULL), "in_state != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_get_display_list(NULL, &list), "inout_view != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_get_display_list(&g_view, NULL), "out_list != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_is_field_pending(NULL), "in_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_get_cell_pixel(0U, 0U, NULL, &coordinate), "out_x != NULL");
    ASSERT_PROBE_EXPECT(game_view_get_cell_pixel(0U, 0U, &coordinate, NULL), "out_y != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_maze(NULL, &map), "inout_view != NULL");
    ASSERT_PROBE_EXPECT(game_view_set_maze(&g_view, NULL), "in_map != NULL");
    ASSERT_PROBE_EXPECT((void)game_view_is_wall_drawn_at(NULL, 0U, 0U), "in_view != NULL");
}
