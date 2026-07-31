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

/* An actor's pixel position: its cell, plus however far it has travelled towards the
 * next one. This is the whole of the interpolation — the rest of it is that the game
 * hands out `progress` at all, and that this function is asked at the frame rate. */
static void prv_get_actor_pixel(const msg_actor_t* const in_actor, int16_t* const out_x, int16_t* const out_y)
{
    const int16_t offset = (int16_t)(((int32_t)in_actor->progress * GAME_VIEW_TILE_SIZE) / GAME_VIEW_FULL_STEP);

    game_view_get_cell_pixel(in_actor->column, in_actor->row, out_x, out_y);

    switch ((direction_e)in_actor->direction)
    {
        case DIRECTION_NORTH: *out_y = (int16_t)(*out_y - offset); break;

        case DIRECTION_SOUTH: *out_y = (int16_t)(*out_y + offset); break;

        case DIRECTION_WEST: *out_x = (int16_t)(*out_x - offset); break;

        case DIRECTION_EAST: *out_x = (int16_t)(*out_x + offset); break;

        default:
            /* Standing still, on its cell. */
            break;
    }
}

/* What a field cell should look like, given the pellets that are left. Walls come from
 * the maze rather than from the state, because they never change during a level and
 * sending 99 unchanging bits every frame would be silly. */
static void prv_describe_cell(const game_view_t* const in_view, uint8_t in_column, uint8_t in_row,
                              sprite_set_id_e* const out_sprite, sprite_set_palette_e* const out_palette)
{
    const cell_t cell = {(int16_t)in_column, (int16_t)in_row};

    if (!playfield_is_walkable(&in_view->maze, cell))
    {
        *out_sprite = SPRITE_SET_TILE;
        *out_palette = SPRITE_SET_PALETTE_WALL;

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

    for (uint8_t index = 0U; index < GAME_VIEW_CELL_COUNT; ++index)
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
            is_frightened ? SPRITE_SET_GHOST_FRIGHTENED : sprite_set_get_ghost_sprite((direction_e)ghost->direction);
        const sprite_set_palette_e palette = is_frightened ? SPRITE_SET_PALETTE_FRIGHTENED : g_ghost_palettes[index];

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

    /* A level change redraws everything; anything else is caught cell by cell while the
     * field is walked. Starting the walk here rather than on demand keeps the decision
     * in one place. */
    if (!inout_view->has_drawn_field || (inout_view->drawn_field.level != in_state->level))
    {
        playfield_load_level(&inout_view->maze, in_state->level);

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
