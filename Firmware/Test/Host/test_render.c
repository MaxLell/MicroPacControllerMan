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

/* The two sprite sizes of the set (§10.2): a field tile is one maze cell, an actor spans
 * two. They used to be one number, and a test that reads "the centre" has to know which of
 * them it is asking about — the centre of an actor is outside a tile drawn at the same
 * place. */
#define TILE  (8)
#define ACTOR (16)

static msg_display_list_t g_list;

static void prv_add(display_item_kind_e in_kind, sprite_set_id_e in_sprite, sprite_set_palette_e in_palette,
                    int16_t in_x, int16_t in_y)
{
    msg_display_item_t* const item = &g_list.items[g_list.count];

    item->kind = (uint8_t)in_kind;
    item->drawing.sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
    item->x = in_x;
    item->y = in_y;

    ++g_list.count;
}

static void prv_clear_list(void)
{
    memset(&g_list, 0, sizeof(g_list));
}

/* The colour at the centre of a sprite drawn at this position, which is where its body
 * always is — whatever else the drawing leaves transparent. */
static framebuffer_color_t prv_centre_of(int16_t in_x, int16_t in_y, int16_t in_size)
{
    return framebuffer_get_pixel(render_get_framebuffer(), (int16_t)(in_x + (in_size / 2)),
                                 (int16_t)(in_y + (in_size / 2)));
}

/* The last rectangle handed to the display port, and how many were handed over. What
 * *reaches the panel* is the whole point of this module on real hardware — the frame
 * buffer can be perfect while the transfer sends four times the bytes it needs to. */
static int16_t g_sent_width;
static int16_t g_sent_height;
static uint8_t g_sent_count;

static void prv_record_region(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                              int16_t in_height, int in_call_count)
{
    (void)in_framebuffer;
    (void)in_x;
    (void)in_y;
    (void)in_call_count;

    g_sent_width = in_width;
    g_sent_height = in_height;
    ++g_sent_count;
}

void setUp(void)
{
    assert_probe_begin();

    display_init_Ignore();
    display_present_Ignore();
    display_present_region_Stub(prv_record_region);

    g_sent_width = 0;
    g_sent_height = 0;
    g_sent_count = 0U;

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
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 40, 40);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 100, 40);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(40, 40, ACTOR));
    TEST_ASSERT_NOT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(100, 40, ACTOR));
}

void test_two_actors_on_one_cell_leave_nothing_behind(void)
{
    /* The regression that a screenshot found and arithmetic then confirmed: four ghosts
     * share three pen cells at the start of every level, so two of them are drawn at the
     * same place. The second one's save-under therefore contains the *first one's
     * sprite*. Unwound in the order they were drawn, that copy goes back onto the panel
     * as though it were background — a ghost of a ghost, which stays there for the rest
     * of the game. Last drawn must be first restored. */
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 60, 60);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_PINKY, 60, 60);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 20, 60);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_PINKY, 140, 60);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(60, 60, ACTOR));
}

void test_a_background_change_survives_the_actor_that_stood_on_it(void)
{
    const framebuffer_color_t pellet_colour = prv_centre_of(0, 0, TILE);

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
    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(80, 80, TILE));
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
    TEST_ASSERT_NOT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(0, 0, TILE));
}

void test_an_actor_may_hang_off_the_edge(void)
{
    /* Entering through a tunnel. Saving and restoring outside the buffer has to be
     * harmless, and the transfer has to be clipped rather than refused. Six pixels off,
     * not ten: the sprite has to straddle the edge while its centre stays on the panel,
     * or the check below would be reading a pixel that does not exist. */
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, -6, 100);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_PACMAN_CLOSED, SPRITE_SET_PALETTE_PACMAN, 10, 100);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(-6, 100, ACTOR));
}

void test_fewer_actors_than_last_frame_still_erases_them_all(void)
{
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 40, 140);
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_INKY, 80, 140);
    render_draw(&g_list);

    prv_clear_list();
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 40, 140);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_HEX16(FRAMEBUFFER_COLOR_BLACK, prv_centre_of(80, 140, ACTOR));
}

/* ==========================================================================
 * what reaches the panel
 * ========================================================================= */

void test_a_field_tile_is_sent_as_a_tile_and_not_as_an_actor(void)
{
    /* The transfer is the expensive half of a frame — 2.08 ms for two small rectangles
     * against 252 ms for a whole one — so sending an 8 x 8 tile inside a 16 x 16 rectangle
     * costs four times what it should. Invisible on a frame that changes one pellet, and a
     * third of a second of black screen on the 868 tiles of a level change. */
    prv_add(DISPLAY_ITEM_BACKGROUND, SPRITE_SET_TILE, SPRITE_SET_PALETTE_WALL, 80, 80);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_UINT8(1U, g_sent_count);
    TEST_ASSERT_EQUAL_INT16(TILE, g_sent_width);
    TEST_ASSERT_EQUAL_INT16(TILE, g_sent_height);
}

void test_a_standing_actor_is_sent_as_one_actor_sized_rectangle(void)
{
    /* The counterpart: an actor really is 16 x 16, and while it is not moving the rectangle
     * spanning where it was and where it is has to collapse onto it rather than grow. */
    prv_add(DISPLAY_ITEM_ACTOR, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_PALETTE_BLINKY, 40, 40);
    render_draw(&g_list);

    TEST_ASSERT_EQUAL_INT16(ACTOR, g_sent_width);
    TEST_ASSERT_EQUAL_INT16(ACTOR, g_sent_height);
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_a_null_list_asserts(void)
{
    ASSERT_PROBE_EXPECT(render_draw(NULL), "in_list != NULL");
}
