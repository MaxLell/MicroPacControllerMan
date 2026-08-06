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

/* Every cell the field handover covers: the maze plus the border drawn round it. */
#define GAME_VIEW_CELL_COUNT (GAME_VIEW_FIELD_WIDTH * GAME_VIEW_FIELD_HEIGHT)

/* The border shifts maze coordinates into the tile array and the field walk. */
#define BORDER               (GAME_VIEW_MAZE_BORDER)

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

/* ---- how a wall is drawn -------------------------------------------------- */

/* The whole of the maze's appearance, in two numbers.
 *
 * A wall is drawn as a **stroke of `WALL_STROKE_WIDTH` pixels, set `WALL_STROKE_INSET`
 * pixels inside the wall's edge**. Six is the weight the arcade's own two-cell walls read at.
 * Five is what leaves a corridor the same clear black on both sides, which is what puts a
 * pellet in the middle of it.
 *
 * There is no tile alphabet any more (DEC-034). There was one, decoded from the 1980 ROM, and
 * it worked for as long as every wall was two cells thick — which is what its pieces were
 * drawn for. A piece has its ink at a fixed place in its 8 pixels, so it can only compose
 * correctly at the thickness it was designed for; every wall of another thickness needed a
 * family of its own, and every seam between two families was a visible fault. Four rounds of
 * them were reported from the panel. Below, the width and the inset are *arithmetic*, so they
 * are the same everywhere by construction rather than by case analysis. */
#define WALL_STROKE_WIDTH      (6)
#define WALL_STROKE_INSET      (5)

/* The ghost house gate: not a wall, but drawn like one and in its own colour. */
#define HOUSE_GATE_ROW         (12)

/* The ghost house, fixed in every maze (`maze_gen`): its wall is the ring rows 12..16 by columns
 * 10..17, and its inside is what that ring encloses.
 *
 * The ring is **one cell thick**, and a 6-pixel stroke set 5 pixels in does not fit in 8 pixels.
 * Centred in the cell instead, it lands half a cell off the grid every other wall sits on — of one
 * maze's strokes, 964 were on the grid and 32 beside it, which is visible — and it leaves the
 * corridor round the house 14 pixels wide where every other corridor is 18.
 *
 * So for **drawing**, the house's wall is two cells thick: the ring, plus the first ring of its
 * inside. It is then an ordinary wall and needs no exception of its own. The ghosts wait in there
 * and are drawn over it, which they already were. The two cells under the gate are left out, or
 * the way through would be drawn shut. */
#define HOUSE_FIRST_COLUMN     (10)
#define HOUSE_LAST_COLUMN      (17)
#define HOUSE_FIRST_ROW        (12)
#define HOUSE_LAST_ROW         (16)
#define HOUSE_GATE_LEFT_COLUMN (13)

/*! \brief Whether a cell is inside the ghost house, its wall included. */
static bool prv_is_in_the_house(int16_t in_x, int16_t in_y)
{
    return (in_x >= HOUSE_FIRST_COLUMN) && (in_x <= HOUSE_LAST_COLUMN) && (in_y >= HOUSE_FIRST_ROW)
           && (in_y <= HOUSE_LAST_ROW);
}

static bool prv_is_house_lining(int16_t in_x, int16_t in_y)
{
    const bool is_inside = (in_x > HOUSE_FIRST_COLUMN) && (in_x < HOUSE_LAST_COLUMN) && (in_y > HOUSE_FIRST_ROW)
                           && (in_y < HOUSE_LAST_ROW);
    const bool is_against_the_ring = (in_x == (HOUSE_FIRST_COLUMN + 1)) || (in_x == (HOUSE_LAST_COLUMN - 1))
                                     || (in_y == (HOUSE_FIRST_ROW + 1)) || (in_y == (HOUSE_LAST_ROW - 1));
    return is_inside && is_against_the_ring;
}

/* The gate, and the cell under it: **wall that happens to be pink**.
 *
 * Left out of the wall it would break the house's top in two, and a broken stroke ends in caps —
 * two blue blocks either side of the opening, which is what the first attempt at this looked like.
 * It is the same shape as the wall around it and differs only in colour, which is also all the
 * arcade does with it. Passable to a ghost either way: that is the map's business, not the
 * picture's. */
static bool prv_is_gate(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    const bool is_gate_column = (in_x == HOUSE_GATE_LEFT_COLUMN) || (in_x == (HOUSE_GATE_LEFT_COLUMN + 1));

    if (!is_gate_column || (in_y < HOUSE_FIRST_ROW) || (in_y > (HOUSE_FIRST_ROW + 1)))
    {
        return false;
    }

    /* The gate's own row is marked in the map; the row under it is the lining below that. */
    return (in_y == (HOUSE_FIRST_ROW + 1)) || (in_map->rows[in_y][in_x] == PLAYFIELD_MAP_GATE);
}

/* Whether there is wall at a cell, counting the border drawn outside the maze.
 *
 * The border **mirrors the maze's edge cell**: beside a wall it is wall, beside a tunnel mouth it
 * is open, so the maze's outer wall is two cells thick and a portal goes cleanly through it
 * (DEC-033). Beyond that one cell there is nothing — which matters, because the geometry below
 * measures how deep a wall is by how far it runs, and a wall that ran on for ever would have no
 * edge to set its stroke in from. */
static bool prv_is_wall(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    int16_t x = in_x;
    int16_t y = in_y;

    if ((in_x < -BORDER) || (in_x >= (PLAYFIELD_WIDTH + BORDER)) || (in_y < -BORDER)
        || (in_y >= (PLAYFIELD_HEIGHT + BORDER)))
    {
        return false;
    }

    x = (x < 0) ? 0 : ((x >= PLAYFIELD_WIDTH) ? (PLAYFIELD_WIDTH - 1) : x);
    y = (y < 0) ? 0 : ((y >= PLAYFIELD_HEIGHT) ? (PLAYFIELD_HEIGHT - 1) : y);

    return (in_map->rows[y][x] == PLAYFIELD_MAP_WALL) || prv_is_house_lining(x, y) || prv_is_gate(in_map, x, y);
}

/* How many cells of wall run on from this one in a direction, not counting it. */
static int16_t prv_get_wall_run(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y, int16_t in_step_x,
                                int16_t in_step_y)
{
    int16_t run = 0;
    int16_t x = (int16_t)(in_x + in_step_x);
    int16_t y = (int16_t)(in_y + in_step_y);

    while (prv_is_wall(in_map, x, y))
    {
        ++run;
        x = (int16_t)(x + in_step_x);
        y = (int16_t)(y + in_step_y);

        /* A run can never be longer than the field; the guard is against a map that is wall
         * all the way across, which the border makes possible. */
        if (run > GAME_VIEW_FIELD_HEIGHT)
        {
            break;
        }
    }

    return run;
}

/* Where the stroke sits, along one axis.
 *
 * `in_thickness` is how deep the wall is on that axis, in pixels. The stroke is centred in it
 * up to the inset: a wall thick enough gets the full 5, and one that is not — the ghost house,
 * whose wall is a single cell — gets what fits, so it is drawn rather than dropped. */
static int16_t prv_get_stroke_inset(int16_t in_thickness)
{
    const int16_t centred = (int16_t)((in_thickness - WALL_STROKE_WIDTH) / 2);

    if (centred < 0)
    {
        return 0;
    }

    if (centred > WALL_STROKE_INSET)
    {
        return WALL_STROKE_INSET;
    }

    return centred;
}

/* Whether a pixel is on the stroke, given how far it is from the wall's edge. */
static bool prv_is_on_stroke(int16_t in_distance, int16_t in_inset)
{
    return (in_distance >= in_inset) && (in_distance < (in_inset + WALL_STROKE_WIDTH));
}

static int16_t prv_get_smaller(int16_t in_first, int16_t in_second)
{
    return (in_first < in_second) ? in_first : in_second;
}

static int16_t prv_get_larger(int16_t in_first, int16_t in_second)
{
    return (in_first > in_second) ? in_first : in_second;
}

/* How far a pixel is from the nearest pixel that is not wall.
 *
 * **Measured to the nearest edge in any direction, corners included** — not as the smallest of
 * the four axis distances, which was the first attempt and left a gap at every junction. Where a
 * branch meets a wall the nearest edge is the inside corner of the join, which is diagonal: to an
 * axis it looks eleven pixels away and it is five, so the branch's stroke stopped short of the
 * one it was meant to run into. A T with a gap in its neck, and the same at every box against the
 * outer wall.
 *
 * Chebyshev distance, so a stroke turns a corner squarely rather than rounding it, and a straight
 * edge gives exactly the same answer as before. Only cells within two of this one are looked at:
 * the stroke reaches ten pixels in at the most, and two cells is sixteen. */
static int16_t prv_get_distance_to_edge(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y,
                                        int16_t in_pixel_x, int16_t in_pixel_y)
{
    const int16_t reach = 2;
    int16_t nearest = (int16_t)(WALL_STROKE_INSET + WALL_STROKE_WIDTH + 1);

    for (int16_t offset_y = -reach; offset_y <= reach; ++offset_y)
    {
        for (int16_t offset_x = -reach; offset_x <= reach; ++offset_x)
        {
            const int16_t left = (int16_t)(offset_x * GAME_VIEW_TILE_SIZE);
            const int16_t top = (int16_t)(offset_y * GAME_VIEW_TILE_SIZE);
            int16_t distance_x;
            int16_t distance_y;

            if (prv_is_wall(in_map, (int16_t)(in_x + offset_x), (int16_t)(in_y + offset_y)))
            {
                continue;
            }

            distance_x = prv_get_larger(0, prv_get_larger((int16_t)(left - in_pixel_x),
                                                          (int16_t)(in_pixel_x - (left + GAME_VIEW_TILE_SIZE - 1))));
            distance_y = prv_get_larger(0, prv_get_larger((int16_t)(top - in_pixel_y),
                                                          (int16_t)(in_pixel_y - (top + GAME_VIEW_TILE_SIZE - 1))));

            nearest = prv_get_smaller(nearest, prv_get_larger(distance_x, distance_y));
        }
    }

    /* Less one, so that 0 means "the outermost pixel of the wall" and the inset above counts the
     * pixels of wall left outside the stroke. Without it every distance is one too many and the
     * stroke sits a pixel further out than asked, which narrows every corridor by two. */
    return (nearest > 0) ? (int16_t)(nearest - 1) : 0;
}

/* The pixels of one wall cell.
 *
 * Everything the old alphabet had a piece for — edges, outer corners, inner corners, junctions,
 * ends, the one-cell walls of the ghost house, the mouths where a tunnel breaks the wall — comes
 * out of the one test below. */
static void prv_get_wall_bitmap(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y,
                                uint8_t* const out_rows)
{
    const int16_t above = (int16_t)(GAME_VIEW_TILE_SIZE * prv_get_wall_run(in_map, in_x, in_y, 0, -1));
    const int16_t below = (int16_t)(GAME_VIEW_TILE_SIZE * prv_get_wall_run(in_map, in_x, in_y, 0, 1));
    const int16_t left = (int16_t)(GAME_VIEW_TILE_SIZE * prv_get_wall_run(in_map, in_x, in_y, -1, 0));
    const int16_t right = (int16_t)(GAME_VIEW_TILE_SIZE * prv_get_wall_run(in_map, in_x, in_y, 1, 0));

    /* How deep the wall is here on each axis, and therefore how far in the stroke may sit. The
     * smaller of the two governs: a wall that is thin one way cannot hold a stroke set further in
     * than it is deep. */
    const int16_t inset = prv_get_smaller(prv_get_stroke_inset((int16_t)(left + right + GAME_VIEW_TILE_SIZE)),
                                          prv_get_stroke_inset((int16_t)(above + below + GAME_VIEW_TILE_SIZE)));

    for (int16_t row = 0; row < GAME_VIEW_TILE_SIZE; ++row)
    {
        uint8_t bits = 0U;

        for (int16_t column = 0; column < GAME_VIEW_TILE_SIZE; ++column)
        {
            if (prv_is_on_stroke(prv_get_distance_to_edge(in_map, in_x, in_y, column, row), inset))
            {
                bits |= (uint8_t)(0x80U >> column);
            }
        }

        out_rows[row] = bits;
    }
}

static void prv_add_item(msg_display_list_t* const inout_list, display_item_kind_e in_kind, sprite_set_id_e in_sprite,
                         sprite_set_palette_e in_palette, int16_t in_x, int16_t in_y)
{
    msg_display_item_t* const item = &inout_list->items[inout_list->count];

    ASSERT(inout_list->count < MSG_DISPLAY_ITEM_MAX);

    item->kind = (uint8_t)in_kind;
    item->drawing.sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
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

    /* Half a cell to the right while an actor is inside the ghost house.
     *
     * The three waiting ghosts are 16 pixels each and stand two cells apart, so they touch; but
     * their centres fall on cell *centres* while the gate's centre — and the house's — fall on a
     * cell *boundary*, four pixels away. No whole cell is at both, so on the grid alone Pinky
     * cannot sit under the gate. This is the four pixels, and it is **drawing only**: the cell an
     * actor stands on is untouched, and so is everything the rules read.
     *
     * It costs a four-pixel sideways step as a ghost leaves the house, where the offset ends. The
     * owner was shown that and chose it over the alternatives (DEC-035). */
    const int16_t house_offset =
        prv_is_in_the_house(in_actor->column, in_actor->row) ? (int16_t)(GAME_VIEW_TILE_SIZE / 2) : 0;

    game_view_get_cell_pixel(in_actor->column, in_actor->row, out_x, out_y);

    *out_x = (int16_t)((*out_x - inset) + house_offset);
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

    /* Straight off the map now. There used to be a second map of the maze to disagree with, and
     * this question was how the two were held together; with one map there is nothing to hold. */
    return in_view->maze.rows[in_row][in_column] == PLAYFIELD_MAP_WALL;
}

/* What one cell of the field is: a piece of wall, the gate, a pellet, or nothing.
 *
 * A wall's pixels are computed here rather than looked up, and they are computed from the map
 * rather than from the game state, because a wall cannot change until the next maze — sending
 * 990 unchanging cells every frame would be silly. */
static void prv_describe_cell(const game_view_t* const in_view, int16_t in_column, int16_t in_row,
                              msg_display_item_t* const out_item)
{
    const bool is_in_maze =
        (in_column >= 0) && (in_column < PLAYFIELD_WIDTH) && (in_row >= 0) && (in_row < PLAYFIELD_HEIGHT);

    /* Out in the border, `prv_is_wall` decides — it mirrors the maze's edge cell, so the border is
     * wall beside a wall and **open beside a tunnel mouth**. Treating every border cell as wall
     * was what drew a wall straight across the mouth, and a portal Pacman cannot walk through is
     * not a portal. */
    const char cell = prv_is_wall(&in_view->maze, in_column, in_row) ? PLAYFIELD_MAP_WALL
                      : is_in_maze                                   ? in_view->maze.rows[in_row][in_column]
                                                                     : PLAYFIELD_MAP_OPEN;

    if (cell == PLAYFIELD_MAP_WALL)
    {
        out_item->kind = (uint8_t)DISPLAY_ITEM_WALL;
        out_item->palette = prv_is_gate(&in_view->maze, in_column, in_row) ? (uint8_t)SPRITE_SET_PALETTE_DOOR
                                                                           : (uint8_t)SPRITE_SET_PALETTE_WALL;
        prv_get_wall_bitmap(&in_view->maze, in_column, in_row, out_item->drawing.wall_rows);

        return;
    }

    out_item->kind = (uint8_t)DISPLAY_ITEM_BACKGROUND;

    if (!is_in_maze || !msg_cell_bitmap_get(in_view->state.has_pellet, (uint8_t)in_column, (uint8_t)in_row))
    {
        out_item->drawing.sprite = (uint8_t)SPRITE_SET_TILE;
        out_item->palette = (uint8_t)SPRITE_SET_PALETTE_EMPTY;

        return;
    }

    out_item->drawing.sprite = msg_cell_bitmap_get(in_view->state.is_power, (uint8_t)in_column, (uint8_t)in_row)
                                   ? (uint8_t)SPRITE_SET_TILE_POWER_PELLET
                                   : (uint8_t)SPRITE_SET_TILE_PELLET;
    out_item->palette = (uint8_t)SPRITE_SET_PALETTE_PELLET;
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
static void prv_add_cell_item(const game_view_t* const in_view, msg_display_list_t* const inout_list, int16_t in_column,
                              int16_t in_row)
{
    msg_display_item_t* const item = &inout_list->items[inout_list->count];

    ASSERT(inout_list->count < MSG_DISPLAY_ITEM_MAX);

    prv_describe_cell(in_view, in_column, in_row, item);
    item->x = (int16_t)(GAME_VIEW_ORIGIN_X + (in_column * GAME_VIEW_TILE_SIZE));
    item->y = (int16_t)(GAME_VIEW_ORIGIN_Y + (in_row * GAME_VIEW_TILE_SIZE));

    ++inout_list->count;
}

/* ---- the HUD ------------------------------------------------------------- */

/* Where each part of the HUD starts in the fixed item list. */
#define HUD_PLAYER_LABEL_INDEX  (0U)
#define HUD_SCORE_INDEX         (HUD_PLAYER_LABEL_INDEX + 3U)
#define HUD_LEVEL_LABEL_INDEX   (HUD_SCORE_INDEX + GAME_VIEW_HUD_SCORE_DIGITS)
#define HUD_LEVEL_INDEX         (HUD_LEVEL_LABEL_INDEX + 5U)
#define HUD_LIVES_INDEX         (HUD_LEVEL_INDEX + GAME_VIEW_HUD_LEVEL_DIGITS)
#define HUD_AI_INDEX            (HUD_LIVES_INDEX + GAME_VIEW_HUD_LIFE_SLOTS)
#define HUD_LOOP_INDEX          (HUD_AI_INDEX + GAME_VIEW_HUD_AI_SLOTS)

/* The maze columns each part sits on. The score runs to column 6 and the level to 26, so
 * both read outward from the edge they belong to, as the arcade's do. */
#define HUD_PLAYER_LABEL_COLUMN (3U)
#define HUD_SCORE_LAST_COLUMN   (6U)
#define HUD_LEVEL_LABEL_COLUMN  (22U)
#define HUD_LEVEL_LAST_COLUMN   (26U)
#define HUD_LIVES_FIRST_COLUMN  (2U)

/* Centred in the label row's own free space: `1UP` ends at column 5 and `LEVEL` starts at 22, so
 * 13 and 14 are the middle of what is left of a 28-column row. */
#define HUD_AI_FIRST_COLUMN     (13U)

/* Right-aligned under `LEVEL`, on the lives row: the lives themselves run from column 2 and take
 * six, so the far end of that row is empty and reads as a second place for a status word. */
#define HUD_LOOP_FIRST_COLUMN   (22U)

static const char* const g_hud_player_label = "1UP";
static const char* const g_hud_level_label = "LEVEL";
static const char* const g_hud_ai_label = "AI";
static const char* const g_hud_loop_label = "LOOP";

_Static_assert(HUD_LOOP_INDEX + GAME_VIEW_HUD_LOOP_SLOTS == GAME_VIEW_HUD_ITEM_COUNT, "HUD item count is wrong");

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

/* What one slot of the HUD should show, for a given view.
 *
 * A pure function of what the view holds, which is the whole trick: what it says is compared
 * against the sprite the slot last actually showed, and the difference is exactly what has to be
 * sent. Reading the view rather than only the state is what lets the AI indication join in
 * without the game having to know that an AI exists. */
static void prv_describe_hud_item(const game_view_t* const in_view, uint8_t in_index, sprite_set_id_e* const out_sprite,
                                  sprite_set_palette_e* const out_palette, int16_t* const out_x, int16_t* const out_y)
{
    ASSERT(in_index < GAME_VIEW_HUD_ITEM_COUNT);

    const msg_game_state_t* const in_state = &in_view->state;

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
    else if (in_index < HUD_AI_INDEX)
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
    else if (in_index < HUD_LOOP_INDEX)
    {
        /* The AI indication (FR-032): `AI` while the agent plays, the font's space while the
         * player does. A space rather than nothing, for the same reason a spent life is a blank. */
        const uint8_t offset = (uint8_t)(in_index - HUD_AI_INDEX);

        prv_set_hud_text_item(in_view->is_ai_active ? g_hud_ai_label[offset] : ' ',
                              (uint8_t)(HUD_AI_FIRST_COLUMN + offset), GAME_VIEW_HUD_LABEL_ROW_Y, out_sprite,
                              out_palette, out_x, out_y);
    }
    else
    {
        /* The loop indication (FR-043): `LOOP` while a finished run will be followed by another. On
         * the lives row rather than beside `AI`, because "who is playing" and "what happens when
         * this run ends" are two different facts and a player reads them at different moments. */
        const uint8_t offset = (uint8_t)(in_index - HUD_LOOP_INDEX);

        prv_set_hud_text_item(in_view->is_infinite ? g_hud_loop_label[offset] : ' ',
                              (uint8_t)(HUD_LOOP_FIRST_COLUMN + offset), GAME_VIEW_HUD_LIVES_Y, out_sprite, out_palette,
                              out_x, out_y);
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

        prv_describe_hud_item(inout_view, index, &sprite, &palette, &x, &y);

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
        const int16_t column = (int16_t)((inout_view->pending_field_cell % GAME_VIEW_FIELD_WIDTH) - BORDER);
        const int16_t row = (int16_t)((inout_view->pending_field_cell / GAME_VIEW_FIELD_WIDTH) - BORDER);

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

    /* Inside the maze only: what changes between frames is a pellet, and the border has none. */
    for (uint16_t index = 0U; index < (PLAYFIELD_WIDTH * PLAYFIELD_HEIGHT); ++index)
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

        prv_add_cell_item(inout_view, inout_list, (int16_t)column, (int16_t)row);
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
     * the moment the player is looking at.
     *
     * Green while the agent has him, yellow while the player does (FR-032). The HUD's three letters
     * say the same thing, and this says it about the thing the eye is already following — which is
     * the difference between knowing and having to look. The *lives* in the HUD stay yellow: they
     * are a count of what is left, not the figure somebody is steering. */
    prv_get_actor_pixel(&in_view->state.pacman, &x, &y);
    prv_add_item(
        inout_list, DISPLAY_ITEM_ACTOR,
        sprite_set_get_pacman_sprite((direction_e)in_view->state.pacman.direction, in_view->state.pacman.progress),
        in_view->is_ai_active ? SPRITE_SET_PALETTE_PACMAN_AI : SPRITE_SET_PALETTE_PACMAN, x, y);
}

/* ==========================================================================
 * game_view - public
 * ========================================================================= */

void game_view_init(game_view_t* inout_view)
{
    ASSERT(inout_view != NULL);

    memset(inout_view, 0, sizeof(*inout_view));
    prv_forget_hud(inout_view);
}

void game_view_set_maze(game_view_t* inout_view, const playfield_map_t* in_map)
{
    ASSERT(inout_view != NULL);
    ASSERT(in_map != NULL);

    inout_view->maze = *in_map;
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

void game_view_set_infinite(game_view_t* inout_view, bool in_is_infinite)
{
    ASSERT(inout_view != NULL);

    inout_view->is_infinite = in_is_infinite;
}

void game_view_set_ai_active(game_view_t* inout_view, bool in_is_active)
{
    ASSERT(inout_view != NULL);

    inout_view->is_ai_active = in_is_active;
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
