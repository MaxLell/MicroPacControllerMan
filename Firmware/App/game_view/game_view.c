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

/* ---- how the maze is drawn ------------------------------------------------ */

/* The ghost house, rows 12..16 and columns 10..17. Its picture is **stamped, not derived**:
 * it is the same structure at the same place in the arcade's maze and in every generated one
 * (`maze_gen`), and it is the only part of the maze with tiles of its own — the pink gate and
 * the four rounded corners exist for nothing else. */
#define HOUSE_FIRST_COLUMN (10)
#define HOUSE_LAST_COLUMN  (17)
#define HOUSE_FIRST_ROW    (12)
#define HOUSE_LAST_ROW     (16)
#define HOUSE_COLUMN_COUNT (HOUSE_LAST_COLUMN - HOUSE_FIRST_COLUMN + 1)
#define HOUSE_ROW_COUNT    (HOUSE_LAST_ROW - HOUSE_FIRST_ROW + 1)

#define NO_TILE            GAME_VIEW_NO_WALL_TILE

/* clang-format off */
static const uint8_t g_house_tiles[HOUSE_ROW_COUNT][HOUSE_COLUMN_COUNT] = {
    {SPRITE_SET_MAZE_HOUSE_TOP_LEFT, SPRITE_SET_MAZE_BOTTOM, SPRITE_SET_MAZE_HOUSE_GATE_LEFT,
     SPRITE_SET_MAZE_HOUSE_GATE, SPRITE_SET_MAZE_HOUSE_GATE, SPRITE_SET_MAZE_HOUSE_GATE_RIGHT,
     SPRITE_SET_MAZE_BOTTOM, SPRITE_SET_MAZE_HOUSE_TOP_RIGHT},
    {SPRITE_SET_MAZE_RIGHT, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, SPRITE_SET_MAZE_LEFT},
    {SPRITE_SET_MAZE_RIGHT, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, SPRITE_SET_MAZE_LEFT},
    {SPRITE_SET_MAZE_RIGHT, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, NO_TILE, SPRITE_SET_MAZE_LEFT},
    {SPRITE_SET_MAZE_HOUSE_BOTTOM_LEFT, SPRITE_SET_MAZE_TOP, SPRITE_SET_MAZE_TOP, SPRITE_SET_MAZE_TOP,
     SPRITE_SET_MAZE_TOP, SPRITE_SET_MAZE_TOP, SPRITE_SET_MAZE_TOP, SPRITE_SET_MAZE_HOUSE_BOTTOM_RIGHT},
};
/* clang-format on */

/* Outside the panel counts as wall: it stops the derivation having a special case for the
 * edge, and it is true — there is nothing out there to walk into. */
static bool prv_is_wall(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    if ((in_x < 0) || (in_x >= PLAYFIELD_WIDTH) || (in_y < 0) || (in_y >= PLAYFIELD_HEIGHT))
    {
        return true;
    }

    return in_map->rows[in_y][in_x] == PLAYFIELD_MAP_WALL;
}

static bool prv_is_open(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    return !prv_is_wall(in_map, in_x, in_y);
}

/* The ring: the wall cells along the panel's edge, which carry the maze's boundary line.
 *
 * The line hugs the edge, so a ring cell only ever does one of three things — run straight,
 * turn where a tunnel breaks the ring, or grow a branch inward where a wall block's own line
 * has to meet it. The branch is what the tee tiles are for, and which tee depends on whether
 * the block starts or ends at this cell, because a block's line is drawn inset: a two-cell
 * thick wall is two parallel lines, and each has to land on its own tee. */
static uint8_t prv_get_ring_tile(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    const bool is_left = in_x == 0;
    const bool is_top = in_y == 0;

    if ((in_x == 0) && (in_y == 0))
    {
        return SPRITE_SET_MAZE_CORNER_TOP_LEFT;
    }

    if ((in_x == (PLAYFIELD_WIDTH - 1)) && (in_y == 0))
    {
        return SPRITE_SET_MAZE_CORNER_TOP_RIGHT;
    }

    if ((in_x == 0) && (in_y == (PLAYFIELD_HEIGHT - 1)))
    {
        return SPRITE_SET_MAZE_CORNER_BOTTOM_LEFT;
    }

    if ((in_x == (PLAYFIELD_WIDTH - 1)) && (in_y == (PLAYFIELD_HEIGHT - 1)))
    {
        return SPRITE_SET_MAZE_CORNER_BOTTOM_RIGHT;
    }

    if ((in_x == 0) || (in_x == (PLAYFIELD_WIDTH - 1)))
    {
        const int16_t inward = is_left ? 1 : -1;

        /* A tunnel mouth breaks the ring, so the line has to stop and turn inward. */
        if (prv_is_open(in_map, in_x, (int16_t)(in_y - 1)))
        {
            return is_left ? SPRITE_SET_MAZE_CORNER_TOP_LEFT : SPRITE_SET_MAZE_CORNER_TOP_RIGHT;
        }

        if (prv_is_open(in_map, in_x, (int16_t)(in_y + 1)))
        {
            return is_left ? SPRITE_SET_MAZE_CORNER_BOTTOM_LEFT : SPRITE_SET_MAZE_CORNER_BOTTOM_RIGHT;
        }

        if (prv_is_wall(in_map, (int16_t)(in_x + inward), in_y))
        {
            if (prv_is_open(in_map, (int16_t)(in_x + inward), (int16_t)(in_y - 1)))
            {
                return is_left ? SPRITE_SET_MAZE_LEFT_TEE_BOTTOM : SPRITE_SET_MAZE_RIGHT_TEE_BOTTOM;
            }

            if (prv_is_open(in_map, (int16_t)(in_x + inward), (int16_t)(in_y + 1)))
            {
                return is_left ? SPRITE_SET_MAZE_LEFT_TEE_TOP : SPRITE_SET_MAZE_RIGHT_TEE_TOP;
            }
        }

        return is_left ? SPRITE_SET_MAZE_LEFT : SPRITE_SET_MAZE_RIGHT;
    }

    {
        const int16_t inward = is_top ? 1 : -1;

        if (prv_is_wall(in_map, in_x, (int16_t)(in_y + inward)))
        {
            if (prv_is_open(in_map, (int16_t)(in_x - 1), (int16_t)(in_y + inward)))
            {
                return is_top ? SPRITE_SET_MAZE_TOP_TEE_RIGHT : SPRITE_SET_MAZE_BOTTOM_TEE_RIGHT;
            }

            if (prv_is_open(in_map, (int16_t)(in_x + 1), (int16_t)(in_y + inward)))
            {
                return is_top ? SPRITE_SET_MAZE_TOP_TEE_LEFT : SPRITE_SET_MAZE_BOTTOM_TEE_LEFT;
            }
        }

        return is_top ? SPRITE_SET_MAZE_TOP : SPRITE_SET_MAZE_BOTTOM;
    }
}

/* A wall block: the cell carries part of that block's own outline, which the arcade draws
 * inset from the block's edge — which is why a thick wall reads as a thin blue rectangle and
 * why the inside of a block is drawn as nothing at all.
 *
 * Four convex corners, four straight edges and four concave corners cover every rectilinear
 * shape, and the ROM has all twelve. The concave ones are found by the diagonals: a cell with
 * no open neighbour but an open corner is where the outline bends around an inside angle. */
static uint8_t prv_get_block_tile(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    const bool north = prv_is_open(in_map, in_x, (int16_t)(in_y - 1));
    const bool south = prv_is_open(in_map, in_x, (int16_t)(in_y + 1));
    const bool west = prv_is_open(in_map, (int16_t)(in_x - 1), in_y);
    const bool east = prv_is_open(in_map, (int16_t)(in_x + 1), in_y);

    if (north && west)
    {
        return SPRITE_SET_MAZE_BLOCK_TOP_LEFT;
    }

    if (north && east)
    {
        return SPRITE_SET_MAZE_BLOCK_TOP_RIGHT;
    }

    if (south && west)
    {
        return SPRITE_SET_MAZE_BLOCK_BOTTOM_LEFT;
    }

    if (south && east)
    {
        return SPRITE_SET_MAZE_BLOCK_BOTTOM_RIGHT;
    }

    if (north)
    {
        return SPRITE_SET_MAZE_BLOCK_TOP;
    }

    if (south)
    {
        return SPRITE_SET_MAZE_BLOCK_BOTTOM;
    }

    if (west)
    {
        return SPRITE_SET_MAZE_BLOCK_LEFT;
    }

    if (east)
    {
        return SPRITE_SET_MAZE_BLOCK_RIGHT;
    }

    if (prv_is_open(in_map, (int16_t)(in_x - 1), (int16_t)(in_y + 1)))
    {
        return SPRITE_SET_MAZE_BLOCK_BOTTOM_INTO_RIGHT;
    }

    if (prv_is_open(in_map, (int16_t)(in_x + 1), (int16_t)(in_y + 1)))
    {
        return SPRITE_SET_MAZE_BLOCK_BOTTOM_INTO_LEFT;
    }

    if (prv_is_open(in_map, (int16_t)(in_x - 1), (int16_t)(in_y - 1)))
    {
        return SPRITE_SET_MAZE_BLOCK_RIGHT_INTO_TOP;
    }

    if (prv_is_open(in_map, (int16_t)(in_x + 1), (int16_t)(in_y - 1)))
    {
        return SPRITE_SET_MAZE_BLOCK_LEFT_INTO_TOP;
    }

    /* Deep inside a block, where the arcade draws nothing. */
    return NO_TILE;
}

static void prv_derive_maze_tiles(game_view_t* const inout_view, const playfield_map_t* const in_map)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const bool is_house = (x >= HOUSE_FIRST_COLUMN) && (x <= HOUSE_LAST_COLUMN) && (y >= HOUSE_FIRST_ROW)
                                  && (y <= HOUSE_LAST_ROW);

            if (is_house)
            {
                inout_view->maze_tiles[y][x] = g_house_tiles[y - HOUSE_FIRST_ROW][x - HOUSE_FIRST_COLUMN];
            }
            else if (!prv_is_wall(in_map, x, y))
            {
                inout_view->maze_tiles[y][x] = NO_TILE;
            }
            else if ((x == 0) || (x == (PLAYFIELD_WIDTH - 1)) || (y == 0) || (y == (PLAYFIELD_HEIGHT - 1)))
            {
                inout_view->maze_tiles[y][x] = prv_get_ring_tile(in_map, x, y);
            }
            else
            {
                inout_view->maze_tiles[y][x] = prv_get_block_tile(in_map, x, y);
            }
        }
    }
}

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

bool game_view_is_wall_drawn_at(const game_view_t* in_view, uint8_t in_column, uint8_t in_row)
{
    ASSERT(in_view != NULL);
    ASSERT(in_column < PLAYFIELD_WIDTH);
    ASSERT(in_row < PLAYFIELD_HEIGHT);

    if (in_view->maze_tiles[in_row][in_column] == NO_TILE)
    {
        return false;
    }

    return !sprite_set_is_maze_gate((sprite_set_id_e)in_view->maze_tiles[in_row][in_column]);
}

/* What a field cell should look like, given the pellets that are left. Walls come from the
 * derived tiles rather than from the state, because they cannot change until the next maze
 * and sending 868 unchanging bits every frame would be silly. */
static void prv_describe_cell(const game_view_t* const in_view, uint8_t in_column, uint8_t in_row,
                              sprite_set_id_e* const out_sprite, sprite_set_palette_e* const out_palette)
{
    if (in_view->maze_tiles[in_row][in_column] != NO_TILE)
    {
        *out_sprite = (sprite_set_id_e)in_view->maze_tiles[in_row][in_column];
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

/* ---- the HUD ------------------------------------------------------------- */

/* Where each part of the HUD starts in the fixed item list. */
#define HUD_PLAYER_LABEL_INDEX  (0U)
#define HUD_SCORE_INDEX         (HUD_PLAYER_LABEL_INDEX + 3U)
#define HUD_LEVEL_LABEL_INDEX   (HUD_SCORE_INDEX + GAME_VIEW_HUD_SCORE_DIGITS)
#define HUD_LEVEL_INDEX         (HUD_LEVEL_LABEL_INDEX + 5U)
#define HUD_LIVES_INDEX         (HUD_LEVEL_INDEX + GAME_VIEW_HUD_LEVEL_DIGITS)

/* The maze columns each part sits on. The score runs to column 6 and the level to 26, so
 * both read outward from the edge they belong to, as the arcade's do. */
#define HUD_PLAYER_LABEL_COLUMN (3U)
#define HUD_SCORE_LAST_COLUMN   (6U)
#define HUD_LEVEL_LABEL_COLUMN  (22U)
#define HUD_LEVEL_LAST_COLUMN   (26U)
#define HUD_LIVES_FIRST_COLUMN  (2U)

static const char* const g_hud_player_label = "1UP";
static const char* const g_hud_level_label = "LEVEL";

_Static_assert(HUD_LIVES_INDEX + GAME_VIEW_HUD_LIFE_SLOTS == GAME_VIEW_HUD_ITEM_COUNT, "HUD item count is wrong");

/* One digit of a number, counting places from the units up, or a space where a leading
 * zero would be. Place zero always gives a digit, so a score of nothing reads `0` rather
 * than as an empty row. */
static char prv_get_digit_character(uint32_t in_value, uint8_t in_place)
{
    uint32_t divisor = 1U;

    for (uint8_t step = 0U; step < in_place; ++step)
    {
        divisor *= 10U;
    }

    if ((in_place > 0U) && (in_value < divisor))
    {
        return ' ';
    }

    return (char)('0' + (char)((in_value / divisor) % 10U));
}

static void prv_set_hud_text_item(char in_character, uint8_t in_column, int16_t in_y, sprite_set_id_e* const out_sprite,
                                  sprite_set_palette_e* const out_palette, int16_t* const out_x, int16_t* const out_y)
{
    *out_sprite = sprite_set_get_glyph(in_character);
    *out_palette = SPRITE_SET_PALETTE_TEXT;
    *out_x = (int16_t)(GAME_VIEW_ORIGIN_X + ((int16_t)in_column * GAME_VIEW_TILE_SIZE));
    *out_y = in_y;
}

/* What one slot of the HUD should show, for a given state.
 *
 * A pure function of the state, which is the whole trick: the same call against the state
 * last drawn says what is on the panel, and the difference between the two is exactly what
 * has to be sent. */
static void prv_describe_hud_item(const msg_game_state_t* const in_state, uint8_t in_index,
                                  sprite_set_id_e* const out_sprite, sprite_set_palette_e* const out_palette,
                                  int16_t* const out_x, int16_t* const out_y)
{
    ASSERT(in_index < GAME_VIEW_HUD_ITEM_COUNT);

    if (in_index < HUD_SCORE_INDEX)
    {
        const uint8_t offset = (uint8_t)(in_index - HUD_PLAYER_LABEL_INDEX);

        prv_set_hud_text_item(g_hud_player_label[offset], (uint8_t)(HUD_PLAYER_LABEL_COLUMN + offset),
                              GAME_VIEW_HUD_LABEL_ROW_Y, out_sprite, out_palette, out_x, out_y);
    }
    else if (in_index < HUD_LEVEL_LABEL_INDEX)
    {
        const uint8_t offset = (uint8_t)(in_index - HUD_SCORE_INDEX);
        const uint8_t place = (uint8_t)(GAME_VIEW_HUD_SCORE_DIGITS - 1U - offset);
        const uint8_t column = (uint8_t)(HUD_SCORE_LAST_COLUMN - place);

        prv_set_hud_text_item(prv_get_digit_character(in_state->score, place), column, GAME_VIEW_HUD_VALUE_ROW_Y,
                              out_sprite, out_palette, out_x, out_y);
    }
    else if (in_index < HUD_LEVEL_INDEX)
    {
        const uint8_t offset = (uint8_t)(in_index - HUD_LEVEL_LABEL_INDEX);

        prv_set_hud_text_item(g_hud_level_label[offset], (uint8_t)(HUD_LEVEL_LABEL_COLUMN + offset),
                              GAME_VIEW_HUD_LABEL_ROW_Y, out_sprite, out_palette, out_x, out_y);
    }
    else if (in_index < HUD_LIVES_INDEX)
    {
        const uint8_t offset = (uint8_t)(in_index - HUD_LEVEL_INDEX);
        const uint8_t place = (uint8_t)(GAME_VIEW_HUD_LEVEL_DIGITS - 1U - offset);
        const uint8_t column = (uint8_t)(HUD_LEVEL_LAST_COLUMN - place);

        prv_set_hud_text_item(prv_get_digit_character(in_state->level, place), column, GAME_VIEW_HUD_VALUE_ROW_Y,
                              out_sprite, out_palette, out_x, out_y);
    }
    else
    {
        /* A life slot: a little Pacman, or a blank the same size once it is spent. Both
         * are drawn, never merely skipped — a slot that stopped being sent would leave the
         * last life on the panel for the rest of the run. */
        const uint8_t slot = (uint8_t)(in_index - HUD_LIVES_INDEX);
        const bool is_alive = slot < in_state->lives;

        *out_sprite = is_alive ? SPRITE_SET_PACMAN_HALF_WEST : SPRITE_SET_ACTOR_BLANK;
        *out_palette = is_alive ? SPRITE_SET_PALETTE_PACMAN : SPRITE_SET_PALETTE_EMPTY;
        *out_x =
            (int16_t)(GAME_VIEW_ORIGIN_X + ((int16_t)(HUD_LIVES_FIRST_COLUMN + (slot * 2U)) * GAME_VIEW_TILE_SIZE));
        *out_y = GAME_VIEW_HUD_LIVES_Y;
    }
}

/* Forget what the HUD showed, so all of it is described again. */
static void prv_forget_hud(game_view_t* const inout_view)
{
    memset(inout_view->drawn_hud, GAME_VIEW_HUD_NOT_DRAWN, sizeof(inout_view->drawn_hud));
}

/* The HUD slots that changed. Same idea as the field, and it shares the same room beside
 * the actors: whatever does not fit this frame is still different next frame. */
static void prv_fill_changed_hud(game_view_t* const inout_view, msg_display_list_t* const inout_list)
{
    const uint8_t room = (uint8_t)(MSG_DISPLAY_ITEM_MAX - MSG_ACTOR_COUNT);

    for (uint8_t index = 0U; index < GAME_VIEW_HUD_ITEM_COUNT; ++index)
    {
        sprite_set_id_e sprite;
        sprite_set_palette_e palette;
        int16_t x;
        int16_t y;

        prv_describe_hud_item(&inout_view->state, index, &sprite, &palette, &x, &y);

        if ((uint8_t)sprite == inout_view->drawn_hud[index])
        {
            continue;
        }

        if (inout_list->count >= room)
        {
            return;
        }

        prv_add_item(inout_list, DISPLAY_ITEM_BACKGROUND, sprite, palette, x, y);
        inout_view->drawn_hud[index] = (uint8_t)sprite;
    }
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
    prv_forget_hud(inout_view);

    /* Not zero: zero is a valid sprite id, so a zeroed maze would draw Pacman's face in
     * every cell of the field. */
    memset(inout_view->maze_tiles, NO_TILE, sizeof(inout_view->maze_tiles));
}

void game_view_set_maze(game_view_t* inout_view, const playfield_map_t* in_map)
{
    ASSERT(inout_view != NULL);
    ASSERT(in_map != NULL);

    prv_derive_maze_tiles(inout_view, in_map);
    inout_view->has_maze = true;

    /* A new maze is every cell changed, so the field goes out again from the start. */
    inout_view->has_drawn_field = false;
    inout_view->pending_field_cell = 0U;
    inout_view->is_full_field_pending = inout_view->has_state;
    prv_forget_hud(inout_view);
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

        /* The HUD goes with it. Not because the maze covers it — it does not — but because
         * a new level changes the number in it, and a reset has to put back a panel that
         * may be showing anything at all. */
        prv_forget_hud(inout_view);
    }
}

bool game_view_get_display_list(game_view_t* inout_view, msg_display_list_t* out_list)
{
    ASSERT(inout_view != NULL);
    ASSERT(out_list != NULL);

    memset(out_list, 0, sizeof(*out_list));

    if (!inout_view->has_state || !inout_view->has_maze)
    {
        return false;
    }

    if (inout_view->is_full_field_pending)
    {
        prv_fill_full_field(inout_view, out_list);

        return out_list->count > 0U;
    }

    prv_fill_changed_cells(inout_view, out_list);
    prv_fill_changed_hud(inout_view, out_list);
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
