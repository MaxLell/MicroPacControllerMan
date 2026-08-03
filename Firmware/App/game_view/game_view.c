#include "game_view.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "msg.h"
#include "playfield.h"
#include "sprite_set.h"

/* ==========================================================================
 * game_view - private
 * ========================================================================= */

#define GAME_VIEW_CELL_COUNT (PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT)

/* A full step, in the 1/256ths of cell_progress_t. */
#define GAME_VIEW_FULL_STEP  (256)

/* Which palette each ghost draws in while it is itself. Indexed by ghost, which is why
 * the order has to match the game's — Blinky, Pinky, Inky, Clyde (§10.4). */
static const sprite_set_palette_e g_ghost_palettes[MSG_GHOST_COUNT] = {
    SPRITE_SET_PALETTE_BLINKY,
    SPRITE_SET_PALETTE_PINKY,
    SPRITE_SET_PALETTE_INKY,
    SPRITE_SET_PALETTE_CLYDE,
};

/* How the maze is *drawn*, one character per cell — the arcade's own map, transcribed
 * unchanged, with `sprite_set` holding what each letter draws.
 *
 * This is the maze a second time, and that is deliberate: `playfield.c` holds what the
 * rules need (wall, pellet, pen, tunnel) and this holds what the panel needs (which of
 * thirty-odd line and corner pieces goes in this cell). Neither can be derived from the
 * other — a wall bitmap does not say which corner piece to draw, and a corner piece does
 * not say whether a ghost may stand there. They come from the same source and a unit test
 * checks they still agree, which is what makes the duplication safe rather than a trap.
 *
 * Everything that is not a wall letter is corridor: `.` and `P` carry pellets and a space
 * carries nothing. What is actually left to eat comes from the state message, not from
 * here — this map never changes. */
/* clang-format off */
static const char* const g_maze_appearance[PLAYFIELD_HEIGHT] = {
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

static void prv_add_item(msg_display_list_t* const inout_list, display_item_kind_e in_kind, sprite_set_id_e in_sprite,
                         sprite_set_palette_e in_palette, int16_t in_x, int16_t in_y)
{
    msg_display_item_t* const item = &inout_list->items[inout_list->count];

    ASSERT(inout_list->count < MSG_DISPLAY_ITEM_MAX);

    item->kind = (uint8_t)in_kind;
    item->sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
    item->reserved = 0U;
    item->x = in_x;
    item->y = in_y;

    ++inout_list->count;
}

/* An actor's pixel position: its cell, minus however much of the step into it is still to
 * run. This is the whole of the interpolation — the rest of it is that the game hands out
 * `progress` at all, and that this function is asked at the frame rate.
 *
 * **Backwards from the cell, not forwards from it.** `direction` is how the actor got
 * here, so the cell it came from is `cell - direction` and known for certain; where it
 * goes next is not decided yet. Drawing forwards along the facing was the old way and it
 * broke every corner — a cell of travel the wrong way and then a snap, or, facing a wall,
 * a full period stood still and then a jump. Drawn this way a corner is simply two
 * straight runs that meet on the corner cell. */
static void prv_get_actor_pixel(const msg_actor_t* const in_actor, int16_t* const out_x, int16_t* const out_y)
{
    const int16_t remaining = (int16_t)(GAME_VIEW_FULL_STEP - in_actor->progress);
    const int16_t offset = (int16_t)(((int32_t)remaining * GAME_VIEW_TILE_SIZE) / GAME_VIEW_FULL_STEP);

    /* An actor is twice a cell wide, so it hangs half a cell out on each side of the one
     * it stands on — which is what makes it look like it occupies the corridor rather
     * than a square in it. */
    const int16_t inset = (int16_t)((GAME_VIEW_ACTOR_SIZE - GAME_VIEW_TILE_SIZE) / 2);

    game_view_get_cell_pixel(in_actor->column, in_actor->row, out_x, out_y);

    *out_x = (int16_t)(*out_x - inset);
    *out_y = (int16_t)(*out_y - inset);

    switch ((direction_e)in_actor->direction)
    {
        case DIRECTION_NORTH: *out_y = (int16_t)(*out_y + offset); break;

        case DIRECTION_SOUTH: *out_y = (int16_t)(*out_y - offset); break;

        case DIRECTION_WEST: *out_x = (int16_t)(*out_x + offset); break;

        case DIRECTION_EAST: *out_x = (int16_t)(*out_x - offset); break;

        default:
            /* Never moved, so there is no step to be part-way through. */
            break;
    }
}

bool game_view_is_wall_drawn_at(uint8_t in_column, uint8_t in_row)
{
    sprite_set_id_e id;

    ASSERT(in_column < PLAYFIELD_WIDTH);
    ASSERT(in_row < PLAYFIELD_HEIGHT);

    if (!sprite_set_get_maze_tile(g_maze_appearance[in_row][in_column], &id))
    {
        return false;
    }

    return !sprite_set_is_maze_gate(id);
}

/* What a field cell should look like, given the pellets that are left. Walls come from the
 * appearance map rather than from the state, because they never change and sending 868
 * unchanging bits every frame would be silly. */
static void prv_describe_cell(const game_view_t* const in_view, uint8_t in_column, uint8_t in_row,
                              sprite_set_id_e* const out_sprite, sprite_set_palette_e* const out_palette)
{
    if (sprite_set_get_maze_tile(g_maze_appearance[in_row][in_column], out_sprite))
    {
        *out_palette = sprite_set_is_maze_gate(*out_sprite) ? SPRITE_SET_PALETTE_DOOR : SPRITE_SET_PALETTE_WALL;

        return;
    }

    if (!msg_cell_bitmap_get(in_view->state.has_pellet, in_column, in_row))
    {
        *out_sprite = SPRITE_SET_TILE;
        *out_palette = SPRITE_SET_PALETTE_EMPTY;

        return;
    }

    *out_sprite = msg_cell_bitmap_get(in_view->state.is_power, in_column, in_row) ? SPRITE_SET_TILE_POWER_PELLET
                                                                                  : SPRITE_SET_TILE_PELLET;
    *out_palette = SPRITE_SET_PALETTE_PELLET;
}

/* Whether a cell looks different from the last time the field was described. */
static bool prv_has_cell_changed(const game_view_t* const in_view, uint8_t in_column, uint8_t in_row)
{
    if (!in_view->has_drawn_field)
    {
        return true;
    }

    if (in_view->drawn_field.level != in_view->state.level)
    {
        return true;
    }

    return (msg_cell_bitmap_get(in_view->state.has_pellet, in_column, in_row)
            != msg_cell_bitmap_get(in_view->drawn_field.has_pellet, in_column, in_row))
           || (msg_cell_bitmap_get(in_view->state.is_power, in_column, in_row)
               != msg_cell_bitmap_get(in_view->drawn_field.is_power, in_column, in_row));
}

/* Hand over field cells until the message is full or the field runs out.
 *
 * A level change is 99 tiles and takes several messages; a swallowed pellet is one. Both
 * go through here, because "the field is not what Render last drew" is the same problem
 * at two sizes. */
static void prv_add_cell_item(const game_view_t* const in_view, msg_display_list_t* const inout_list, uint8_t in_column,
                              uint8_t in_row)
{
    sprite_set_id_e sprite;
    sprite_set_palette_e palette;
    int16_t x;
    int16_t y;

    prv_describe_cell(in_view, in_column, in_row, &sprite, &palette);
    game_view_get_cell_pixel(in_column, in_row, &x, &y);
    prv_add_item(inout_list, DISPLAY_ITEM_BACKGROUND, sprite, palette, x, y);
}

/* Hand over a whole field, a message at a time, keeping our place between calls.
 *
 * This is the level-change path only. It deliberately says nothing about the actors: a
 * transition is not a frame, and drawing Pacman onto a maze that is still half the old
 * one would look worse than not drawing him for a moment. */
static void prv_fill_full_field(game_view_t* const inout_view, msg_display_list_t* const inout_list)
{
    while ((inout_view->pending_field_cell < GAME_VIEW_CELL_COUNT) && (inout_list->count < MSG_DISPLAY_ITEM_MAX))
    {
        const uint8_t column = (uint8_t)(inout_view->pending_field_cell % PLAYFIELD_WIDTH);
        const uint8_t row = (uint8_t)(inout_view->pending_field_cell / PLAYFIELD_WIDTH);

        ++inout_view->pending_field_cell;

        prv_add_cell_item(inout_view, inout_list, column, row);
    }

    if (inout_view->pending_field_cell >= GAME_VIEW_CELL_COUNT)
    {
        inout_view->is_full_field_pending = false;
        inout_view->drawn_field = inout_view->state;
        inout_view->has_drawn_field = true;
    }
}

/* The cells that changed since the last frame — in practice the one pellet Pacman just
 * swallowed, and usually none at all. They ride along with the actors, because a pellet
 * that vanishes a frame before or after the actor that ate it is a visible glitch. */
static void prv_fill_changed_cells(game_view_t* const inout_view, msg_display_list_t* const inout_list)
{
    const uint8_t room = (uint8_t)(MSG_DISPLAY_ITEM_MAX - MSG_ACTOR_COUNT);

    for (uint16_t index = 0U; index < GAME_VIEW_CELL_COUNT; ++index)
    {
        const uint8_t column = (uint8_t)(index % PLAYFIELD_WIDTH);
        const uint8_t row = (uint8_t)(index / PLAYFIELD_WIDTH);

        if (!prv_has_cell_changed(inout_view, column, row))
        {
            continue;
        }

        if (inout_list->count >= room)
        {
            /* More changed at once than a frame can carry. Nothing is lost: the rest are
             * still different from `drawn_field`, so they go out next frame. */
            return;
        }

        prv_add_cell_item(inout_view, inout_list, column, row);
        msg_cell_bitmap_set(inout_view->drawn_field.has_pellet, column, row,
                            msg_cell_bitmap_get(inout_view->state.has_pellet, column, row));
        msg_cell_bitmap_set(inout_view->drawn_field.is_power, column, row,
                            msg_cell_bitmap_get(inout_view->state.is_power, column, row));
    }
}

static void prv_fill_actor_items(const game_view_t* const in_view, msg_display_list_t* const inout_list)
{
    int16_t x;
    int16_t y;

    for (uint8_t index = 0U; index < MSG_GHOST_COUNT; ++index)
    {
        const bool is_frightened = (in_view->state.frightened_ghosts & (uint8_t)(1U << index)) != 0U;
        const msg_actor_t* const ghost = &in_view->state.ghosts[index];
        const sprite_set_id_e sprite =
            is_frightened ? sprite_set_get_frightened_sprite(ghost->progress)
                          : sprite_set_get_ghost_sprite((direction_e)ghost->direction, ghost->progress);
        /* The game decides *whether* the window is flashing and on which half of the
         * flash it currently is; all that is left here is which palette that means. */
        sprite_set_palette_e palette = g_ghost_palettes[index];

        if (is_frightened)
        {
            palette = in_view->state.are_frightened_ghosts_flashing ? SPRITE_SET_PALETTE_FRIGHTENED_FLASH
                                                                    : SPRITE_SET_PALETTE_FRIGHTENED;
        }

        prv_get_actor_pixel(ghost, &x, &y);
        prv_add_item(inout_list, DISPLAY_ITEM_ACTOR, sprite, palette, x, y);
    }

    /* Pacman last, so he is on top where he and a ghost share a cell — which is exactly
     * the moment the player is looking at. */
    prv_get_actor_pixel(&in_view->state.pacman, &x, &y);
    prv_add_item(
        inout_list, DISPLAY_ITEM_ACTOR,
        sprite_set_get_pacman_sprite((direction_e)in_view->state.pacman.direction, in_view->state.pacman.progress),
        SPRITE_SET_PALETTE_PACMAN, x, y);
}

/* ==========================================================================
 * game_view - public
 * ========================================================================= */

void game_view_init(game_view_t* inout_view)
{
    ASSERT(inout_view != NULL);

    memset(inout_view, 0, sizeof(*inout_view));
}

void game_view_set_state(game_view_t* inout_view, const msg_game_state_t* in_state)
{
    ASSERT(inout_view != NULL);
    ASSERT(in_state != NULL);

    inout_view->state = *in_state;
    inout_view->has_state = true;

    /* A level change redraws everything — the maze is the same one, but every pellet is
     * back. Anything else is caught cell by cell while the field is walked; starting the
     * walk here rather than on demand keeps the decision in one place. */
    if (!inout_view->has_drawn_field || (inout_view->drawn_field.level != in_state->level))
    {
        inout_view->has_drawn_field = false;
        inout_view->pending_field_cell = 0U;
        inout_view->is_full_field_pending = true;
    }
}

bool game_view_get_display_list(game_view_t* inout_view, msg_display_list_t* out_list)
{
    ASSERT(inout_view != NULL);
    ASSERT(out_list != NULL);

    memset(out_list, 0, sizeof(*out_list));

    if (!inout_view->has_state)
    {
        return false;
    }

    if (inout_view->is_full_field_pending)
    {
        prv_fill_full_field(inout_view, out_list);

        return out_list->count > 0U;
    }

    prv_fill_changed_cells(inout_view, out_list);
    prv_fill_actor_items(inout_view, out_list);

    return true;
}

bool game_view_is_field_pending(const game_view_t* in_view)
{
    ASSERT(in_view != NULL);

    return in_view->is_full_field_pending;
}

void game_view_get_cell_pixel(uint8_t in_column, uint8_t in_row, int16_t* out_x, int16_t* out_y)
{
    ASSERT(out_x != NULL);
    ASSERT(out_y != NULL);

    *out_x = (int16_t)(GAME_VIEW_ORIGIN_X + (in_column * GAME_VIEW_TILE_SIZE));
    *out_y = (int16_t)(GAME_VIEW_ORIGIN_Y + (in_row * GAME_VIEW_TILE_SIZE));
}
