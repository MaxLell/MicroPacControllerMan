/*
 * test_render.c
 *
 * Save-under, which is the whole of this module and is invisible until it is wrong.
 *
 * The display port is mocked away: what matters here is what ends up in the frame
 * buffer, not which rectangles were transferred. The transfer is measured on the board
 * (`ott animation`), and mocking it keeps these tests about erasing.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "framebuffer.h"
#include "mock_display.h"
#include "msg.h"
#include "render.h"
#include "sprite.h"
#include "sprite_set.h"

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define TILE (20)

static msg_display_list_t g_list;

static void prv_add(display_item_kind_e in_kind, sprite_set_id_e in_sprite, sprite_set_palette_e in_palette,
                    int16_t in_x, int16_t in_y)
{
    msg_display_item_t* const item = &g_list.items[g_list.count];

    item->kind = (uint8_t)in_kind;
    item->sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
    item->reserved = 0U;
    item->x = in_x;
    item->y = in_y;

    ++g_list.count;
}

static void prv_clear_list(void)
{
    memset(&g_list, 0, sizeof(g_list));
}

/* The colour at a cell's centre, which is where a sprite's body always is. */
static framebuffer_color_t prv_centre_of(int16_t in_x, int16_t in_y)
{
    return framebuffer_get_pixel(render_get_framebuffer(), (int16_t)(in_x + (TILE / 2)), (int16_t)(in_y + (TILE / 2)));
}

void setUp(void)
{
    assert_probe_begin();

    display_init_Ignore();
    display_present_Ignore();
    display_present_region_Ignore();

    render_init();
    prv_clear_list();
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * erasing
 * ========================================================================= */

void test_an_actor_leaves_nothing_behind_when_it_moves(void)
{
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 40, 40);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 100, 40);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(40, 40));
    TEST_ASSERT_NOT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(100, 40));
}

void test_two_actors_on_one_cell_leave_nothing_behind(void)
{
    /* The regression that a screenshot found and arithmetic then confirmed: four ghosts
     * share three pen cells at the start of every level, so two of them are drawn at the
     * same place. The second one's save-under therefore contains the *first one's
     * sprite*. Unwound in the order they were drawn, that copy goes back onto the panel
     * as though it were background — a ghost of a ghost, which stays there for the rest
     * of the game. Last drawn must be first restored. */
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 60, 60);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_PINKY, 60, 60);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 20, 60);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_PINKY, 140, 60);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(60, 60));
}

void test_a_background_change_survives_the_actor_that_stood_on_it(void)
{
    const framebuffer_color_t pellet_colour = prv_centre_of(0, 0);

    /* A pellet, then an actor standing on it. */
    prv_add(DISPLAY_ITEM_BACKGROUND, SPRITE_SET_TILE_PELLET, SPRITE_SET_PALETTE_PELLET, 80, 80);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, 80, 80);
    render_draw(&g_list);

    /* He eats it and moves on: the cell goes out as an empty tile in the same frame. */
    prv_clear_list();
    prv_add(DISPLAY_ITEM_BACKGROUND, SPRITE_SET_TILE, SPRITE_SET_PALETTE_EMPTY, 80, 80);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, 100, 80);
    render_draw(&g_list);

    /* Without the background item winning over the restore, the dot would come back. */
    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(80, 80));
    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, pellet_colour);
}

void test_the_field_is_not_erased_between_frames(void)
{
    prv_add(DISPLAY_ITEM_BACKGROUND, SPRITE_SET_TILE, SPRITE_SET_PALETTE_WALL, 0, 0);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, 100, 100);
    render_draw(&g_list);

    /* A wall is drawn once per level and has to stay: only actors are transient. */
    TEST_ASSERT_NOT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(0, 0));
}

void test_an_actor_may_hang_off_the_edge(void)
{
    /* Entering through a tunnel. Saving and restoring outside the buffer has to be
     * harmless, and the transfer has to be clipped rather than refused. */
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, -10, 100);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, 10, 100);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(-10, 100));
}

void test_fewer_actors_than_last_frame_still_erases_them_all(void)
{
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 40, 140);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_INKY, 80, 140);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST, SPRITE_SET_PALETTE_BLINKY, 40, 140);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(80, 140));
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_a_null_list_asserts(void)
{
    ASSERT_PROBE_EXPECT(render_draw(NULL), "in_list != NULL");
}
