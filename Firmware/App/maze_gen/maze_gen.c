#include "maze_gen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "playfield.h"

/* ==========================================================================
 * maze_gen - private
 * ========================================================================= */

/* The small grid the pieces are stacked on: the middle column plus the right half of the
 * maze. Everything else follows from it — the maze is this, upscaled by three and mirrored. */
#define GRID_COLUMNS    (5)
#define GRID_ROWS       (9)
#define GRID_CELL_COUNT (GRID_COLUMNS * GRID_ROWS)

/* The four sides of a cell, in the original's order — the code turns a side around with
 * `(side + 2) % SIDE_COUNT`, which only works if opposites are two apart. */
#define SIDE_UP         (0)
#define SIDE_RIGHT      (1)
#define SIDE_DOWN       (2)
#define SIDE_LEFT       (3)
#define SIDE_COUNT      (4)

/* Stands in for JavaScript's `undefined` wherever the original passes a missing side, cell
 * or size around and relies on every comparison against it being false. */
#define NONE            (-1)

/* Groups number the contiguous pieces. `GROUP_NONE` is every cell that was never filled
 * *and* the four cells of the ghost house, which the original fills without numbering — so
 * they compare equal to each other and unequal to every real group, which is what decides
 * where a path tile goes. */
#define GROUP_NONE      (0xFFU)

/* The tile grid, before mirroring: one row per maze row, `SUB_COLUMNS` of the right half. */
#define SUB_ROWS        ((GRID_ROWS * 3) + 1 + 3)
#define SUB_COLUMNS     ((GRID_COLUMNS * 3) - 1 + 2)
#define MIRROR_COLUMN   (SUB_COLUMNS - 2)
#define FULL_COLUMNS    ((SUB_COLUMNS - 2) * 2)

/* The maze's size is not a free choice: it falls out of the 9 x 5 grid upscaled by three
 * and mirrored, and it has to be the size `playfield` works in. */
_Static_assert(SUB_ROWS == PLAYFIELD_HEIGHT, "the upscaled grid is the maze's height");
_Static_assert(FULL_COLUMNS == PLAYFIELD_WIDTH, "the mirrored grid is the maze's width");

/* The generator's own tile alphabet, which is not `playfield`'s: it distinguishes a wall
 * from the blank inside a wall, and a path with a pellet from one without. The last step
 * translates it. */
#define TILE_BLANK                    '_'
#define TILE_PATH                     '.'
#define TILE_WALL                     '|'
#define TILE_DOOR                     '-'
#define TILE_ENERGIZER                'o'
#define TILE_EMPTY                    ' '

/* Probabilities, as the 32-bit thresholds `prv_take_chance` compares against: the original
 * asks `Math.random() <= p`, and `Math.random()` is this generator's next word over 2^32,
 * so the test is exactly `word <= p * 2^32`. Written out rather than computed so the
 * arithmetic is visible and nothing here needs floating point. */
#define CHANCE_NEVER                  (0ULL)          /* 0.00 */
#define CHANCE_STOP_GROWING_AT_SIZE_2 (429496729ULL)  /* 0.10 */
#define CHANCE_STOP_GROWING_AT_SIZE_3 (2147483648ULL) /* 0.50 */
#define CHANCE_STOP_GROWING_AT_SIZE_4 (3221225472ULL) /* 0.75 */
#define CHANCE_ALWAYS                 (4294967296ULL) /* 1.00 */
#define CHANCE_TOP_BOTTOM_SINGLE_JOIN (1503238553ULL) /* 0.35 */
#define CHANCE_JOIN_TO_BOUNDARY       (1073741824ULL) /* 0.25 */
#define CHANCE_JOIN_TO_RIGHT_BOUNDARY (2147483648ULL) /* 0.50 */
#define CHANCE_EXTEND_AT_SIZE_3_OR_4  (2147483648ULL) /* 0.50 */
#define CHANCE_TWO_TUNNELS            (1932735283ULL) /* 0.45 */

#define MAX_PIECE_SIZE                (5)
#define MAX_LONG_PIECES               (1U)

/* Where the arcade puts the things the maze is built around. The generator's own grid
 * already places its ghost house here — checked over 2000 seeds — so stamping these in
 * confirms the layout rather than forcing it. */
#define HOUSE_FIRST_INTERIOR_COLUMN   (11)
#define HOUSE_LAST_INTERIOR_COLUMN    (16)
#define HOUSE_FIRST_INTERIOR_ROW      (13)
#define HOUSE_LAST_INTERIOR_ROW       (15)
#define HOUSE_GATE_ROW                (12)
#define HOUSE_GATE_LEFT_COLUMN        (13)
#define HOUSE_GATE_RIGHT_COLUMN       (14)
#define BLINKY_START_COLUMN           (14)
#define BLINKY_START_ROW              (11)
#define PACMAN_START_COLUMN           (13)
#define PACMAN_START_ROW              (23)

/* The three ghosts that wait inside, at the arcade's own cells on the middle row of the
 * house: Pinky in the centre, Inky to the left, Clyde to the right. */
#define PINKY_START_COLUMN            (13)
#define INKY_START_COLUMN             (11)
#define CLYDE_START_COLUMN            (16)
#define HOUSE_GHOST_ROW               (14)

/* A retry budget rather than `while (true)`: the original loops until it likes what it
 * built, which is right, but on a target an unbounded loop with no way out is not. Three
 * attempts is the average and 24 the worst of 2000 seeds, so this is a fault ceiling, not
 * a tuning knob — reaching it means the generator is broken, and an assertion says so. */
#define MAX_ATTEMPTS                  (256U)

typedef struct
{
    int8_t x;
    int8_t y;
    bool is_filled;
    bool connect[SIDE_COUNT];
    uint8_t group;

    /* Where this cell lands once the grid is upscaled, and how big it is there. */
    int8_t final_x;
    int8_t final_y;
    int8_t final_width;
    int8_t final_height;

    /* Whether this cell *could* be stretched or narrowed, and whether it was chosen. One
     * row is made a cell taller and one column a cell narrower, which is what stops the
     * maze reading as a grid of identical boxes. */
    bool can_raise_height;
    bool can_shrink_width;
    bool raise_height;
    bool shrink_width;

    /* Tunnel bookkeeping: which right-edge cells could carry a tunnel mouth, and which
     * one does. */
    bool is_top_tunnel;
    bool is_void_tunnel_candidate;
    bool is_single_dead_end_candidate;
    int8_t single_dead_end_side;
    bool is_double_dead_end_candidate;
    bool is_edge_tunnel_candidate;
} prv_cell_t;

typedef struct
{
    uint32_t random_state;

    prv_cell_t cells[GRID_CELL_COUNT];

    /* Which row of each column was made taller, and which column of each row narrower;
     * `NONE` where none was. Deliberately *not* cleared between attempts — see
     * #prv_generate_grid. */
    int8_t tall_row_of_column[GRID_COLUMNS];
    int8_t narrow_column_of_row[GRID_ROWS];

    uint8_t group_count;
    uint8_t long_piece_count;
    uint8_t top_single_cell_count;
    uint8_t bottom_single_cell_count;

    /* The upscaled, mirrored tile grid, and which cell each tile of the right half came
     * from. The cell grid is indexed two to the left of its first column, exactly as the
     * original's is, so the offset is carried here rather than fixed up at every use. */
    char tiles[SUB_ROWS][FULL_COLUMNS];
    int8_t tile_cell[(SUB_ROWS * SUB_COLUMNS) + 2];
} prv_generator_t;

/* ---- random numbers ------------------------------------------------------- */

/* xorshift32. The generator's quality does not matter here — reproducibility does, and
 * this is the shortest thing that is identical in C and in the JavaScript the port is
 * checked against. */
static uint32_t prv_next_word(prv_generator_t* const inout_generator)
{
    uint32_t state = inout_generator->random_state;

    state ^= (uint32_t)(state << 13);
    state ^= (uint32_t)(state >> 17);
    state ^= (uint32_t)(state << 5);

    inout_generator->random_state = state;

    return state;
}

/* An integer in `[in_minimum, in_maximum]`, as `Math.floor(Math.random() * span) + min`
 * does it — the multiply is done in 64 bits so the result matches exactly. */
static int8_t prv_random_int(prv_generator_t* const inout_generator, int8_t in_minimum, int8_t in_maximum)
{
    const uint64_t span = (uint64_t)((int32_t)in_maximum - (int32_t)in_minimum + 1);
    const uint64_t word = (uint64_t)prv_next_word(inout_generator);

    ASSERT(in_maximum >= in_minimum);

    return (int8_t)((int32_t)in_minimum + (int32_t)((word * span) >> 32));
}

/* Whether an event of the given probability happens. */
static bool prv_take_chance(prv_generator_t* const inout_generator, uint64_t in_threshold)
{
    return (uint64_t)prv_next_word(inout_generator) <= in_threshold;
}

/* The original's shuffle, which is not a Fisher-Yates and is biased. Kept as it is: it
 * draws one number per element, and the port has to draw the same ones in the same order. */
static void prv_shuffle(prv_generator_t* const inout_generator, int8_t* const inout_list, uint8_t in_count)
{
    for (uint8_t index = 0U; index < in_count; ++index)
    {
        const uint8_t other = (uint8_t)prv_random_int(inout_generator, 0, (int8_t)(in_count - 1U));
        const int8_t swapped = inout_list[index];

        inout_list[index] = inout_list[other];
        inout_list[other] = swapped;
    }
}

/* One element of a list, or `NONE` for an empty one. Draws a number only when there is
 * something to choose, which is what keeps the sequence in step with the original. */
static int8_t prv_random_element(prv_generator_t* const inout_generator, const int8_t* const in_list, uint8_t in_count)
{
    if (in_count == 0U)
    {
        return NONE;
    }

    return in_list[(uint8_t)prv_random_int(inout_generator, 0, (int8_t)(in_count - 1U))];
}

/* ---- the grid ------------------------------------------------------------- */

static int8_t prv_get_neighbour(const prv_generator_t* const in_generator, int8_t in_index, int8_t in_side)
{
    const prv_cell_t* const cell = &in_generator->cells[in_index];

    ASSERT(in_index >= 0);

    switch (in_side)
    {
        case SIDE_LEFT: return (cell->x > 0) ? (int8_t)(in_index - 1) : NONE;
        case SIDE_RIGHT: return (cell->x < (GRID_COLUMNS - 1)) ? (int8_t)(in_index + 1) : NONE;
        case SIDE_UP: return (cell->y > 0) ? (int8_t)(in_index - GRID_COLUMNS) : NONE;
        case SIDE_DOWN: return (cell->y < (GRID_ROWS - 1)) ? (int8_t)(in_index + GRID_COLUMNS) : NONE;
        default: ASSERT(false); break;
    }

    return NONE;
}

/* Whether a cell is connected on a side, treating "no such cell" as not connected. */
static bool prv_is_connected(const prv_generator_t* const in_generator, int8_t in_index, int8_t in_side)
{
    if (in_index < 0)
    {
        return false;
    }

    return in_generator->cells[in_index].connect[in_side];
}

static bool prv_is_cross_centre(const prv_generator_t* const in_generator, int8_t in_index)
{
    const prv_cell_t* const cell = &in_generator->cells[in_index];

    return cell->connect[SIDE_UP] && cell->connect[SIDE_RIGHT] && cell->connect[SIDE_DOWN] && cell->connect[SIDE_LEFT];
}

/* Start over: an empty grid with the ghost house already in it. The house is placed rather
 * than grown, because everything about the game that happens at a fixed place happens
 * there — where a ghost is released, where an eaten one revives, which cell Pacman may
 * never enter. */
static void prv_reset(prv_generator_t* const inout_generator)
{
    int8_t index;
    prv_cell_t* cell;

    memset(inout_generator->cells, 0, sizeof(inout_generator->cells));

    for (index = 0; index < GRID_CELL_COUNT; ++index)
    {
        cell = &inout_generator->cells[index];
        cell->x = (int8_t)(index % GRID_COLUMNS);
        cell->y = (int8_t)(index / GRID_COLUMNS);
        cell->group = GROUP_NONE;
        cell->single_dead_end_side = NONE;
    }

    inout_generator->long_piece_count = 0U;
    inout_generator->top_single_cell_count = 0U;
    inout_generator->bottom_single_cell_count = 0U;

    index = 3 * GRID_COLUMNS;
    cell = &inout_generator->cells[index];
    cell->is_filled = true;
    cell->connect[SIDE_LEFT] = true;
    cell->connect[SIDE_RIGHT] = true;
    cell->connect[SIDE_DOWN] = true;

    ++index;
    cell = &inout_generator->cells[index];
    cell->is_filled = true;
    cell->connect[SIDE_LEFT] = true;
    cell->connect[SIDE_DOWN] = true;

    index = (int8_t)(index + GRID_COLUMNS - 1);
    cell = &inout_generator->cells[index];
    cell->is_filled = true;
    cell->connect[SIDE_LEFT] = true;
    cell->connect[SIDE_UP] = true;
    cell->connect[SIDE_RIGHT] = true;

    ++index;
    cell = &inout_generator->cells[index];
    cell->is_filled = true;
    cell->connect[SIDE_UP] = true;
    cell->connect[SIDE_LEFT] = true;
}

/* ---- stacking the pieces -------------------------------------------------- */

/* Every empty cell of the leftmost column that still has one. Growth always starts at the
 * left, which is what makes the stacking fill the grid instead of leaving holes. */
static uint8_t prv_get_leftmost_empty_cells(const prv_generator_t* const in_generator, int8_t* const out_list)
{
    uint8_t count = 0U;

    for (int8_t x = 0; x < GRID_COLUMNS; ++x)
    {
        for (int8_t y = 0; y < GRID_ROWS; ++y)
        {
            const int8_t index = (int8_t)(x + (y * GRID_COLUMNS));

            if (!in_generator->cells[index].is_filled)
            {
                out_list[count] = index;
                ++count;
            }
        }

        if (count > 0U)
        {
            break;
        }
    }

    return count;
}

/* Whether a piece may grow from a cell towards a side.
 *
 * `in_previous_side` and `in_size` may be `NONE`, which is how the original calls this when
 * it is only asking "is there room that way" and not "would that make an ugly piece". */
static bool prv_is_open_cell(const prv_generator_t* const in_generator, int8_t in_index, int8_t in_side,
                             int8_t in_previous_side, int8_t in_size)
{
    const prv_cell_t* const cell = &in_generator->cells[in_index];
    int8_t next;
    int8_t next_left;

    /* Never wall in the corridor Pacman starts in. */
    if (((cell->y == 6) && (cell->x == 0) && (in_side == SIDE_DOWN))
        || ((cell->y == 7) && (cell->x == 0) && (in_side == SIDE_UP)))
    {
        return false;
    }

    /* No straight run of three: at size two, refuse to carry on the way we came or to
     * double back. A three-long straight piece reads as a wall rather than as a shape. */
    if ((in_size == 2) && ((in_side == in_previous_side) || (((in_side + 2) % SIDE_COUNT) == in_previous_side)))
    {
        return false;
    }

    next = prv_get_neighbour(in_generator, in_index, in_side);

    if ((next < 0) || in_generator->cells[next].is_filled)
    {
        return false;
    }

    /* Only grow into a cell whose own left neighbour is already filled, so a piece never
     * leaves an unfillable pocket behind it. */
    next_left = prv_get_neighbour(in_generator, next, SIDE_LEFT);

    return (next_left < 0) || in_generator->cells[next_left].is_filled;
}

static uint8_t prv_get_open_sides(const prv_generator_t* const in_generator, int8_t in_index, int8_t in_previous_side,
                                  int8_t in_size, int8_t* const out_sides)
{
    uint8_t count = 0U;

    for (int8_t side = 0; side < SIDE_COUNT; ++side)
    {
        if (prv_is_open_cell(in_generator, in_index, side, in_previous_side, in_size))
        {
            out_sides[count] = side;
            ++count;
        }
    }

    return count;
}

static void prv_connect_cell(prv_generator_t* const inout_generator, int8_t in_index, int8_t in_side)
{
    const int8_t next = prv_get_neighbour(inout_generator, in_index, in_side);

    ASSERT(next >= 0);

    inout_generator->cells[in_index].connect[in_side] = true;
    inout_generator->cells[next].connect[(in_side + 2) % SIDE_COUNT] = true;

    /* The middle column is the mirror axis, so a piece opening right there opens left too —
     * otherwise the two halves would not meet. */
    if ((inout_generator->cells[in_index].x == 0) && (in_side == SIDE_RIGHT))
    {
        inout_generator->cells[in_index].connect[SIDE_LEFT] = true;
    }
}

static void prv_fill_cell(prv_generator_t* const inout_generator, int8_t in_index)
{
    inout_generator->cells[in_index].is_filled = true;
    inout_generator->cells[in_index].group = inout_generator->group_count;
}

/* Try to turn a horizontal two-cell piece into a four-cell "L".
 *
 * Kept as its own step because it cannot be reached later: by size three the piece has
 * already committed to a shape. Returns whether the piece was extended and is finished. */
static bool prv_try_extend_to_long_l(prv_generator_t* const inout_generator, int8_t in_first_cell)
{
    const int8_t first_right = prv_get_neighbour(inout_generator, in_first_cell, SIDE_RIGHT);
    int8_t corner;
    int8_t side = NONE;
    bool can_go_up;
    bool can_go_down;

    if ((inout_generator->cells[in_first_cell].x <= 0) || !inout_generator->cells[in_first_cell].connect[SIDE_RIGHT]
        || (first_right < 0))
    {
        return false;
    }

    corner = prv_get_neighbour(inout_generator, first_right, SIDE_RIGHT);

    if (corner < 0)
    {
        return false;
    }

    if ((inout_generator->long_piece_count >= MAX_LONG_PIECES) || !prv_take_chance(inout_generator, CHANCE_ALWAYS))
    {
        return false;
    }

    can_go_up = prv_is_open_cell(inout_generator, corner, SIDE_UP, NONE, NONE);
    can_go_down = prv_is_open_cell(inout_generator, corner, SIDE_DOWN, NONE, NONE);

    if (can_go_up && can_go_down)
    {
        side = (prv_random_int(inout_generator, 0, 1) == 0) ? SIDE_UP : SIDE_DOWN;
    }
    else if (can_go_up)
    {
        side = SIDE_UP;
    }
    else if (can_go_down)
    {
        side = SIDE_DOWN;
    }
    else
    {
        return false;
    }

    prv_connect_cell(inout_generator, corner, SIDE_LEFT);
    prv_fill_cell(inout_generator, corner);
    prv_connect_cell(inout_generator, corner, side);
    prv_fill_cell(inout_generator, prv_get_neighbour(inout_generator, corner, side));
    ++inout_generator->long_piece_count;

    return true;
}

/* Give a finished three- or four-cell piece a long leg, sometimes. This is what produces
 * the occasional "T", and the limit of one long piece per maze is what keeps it occasional. */
static void prv_try_extend_leg(prv_generator_t* const inout_generator, int8_t in_first_cell, int8_t in_cell)
{
    int8_t sides[SIDE_COUNT];
    uint8_t count = 0U;
    int8_t side;
    int8_t neighbour;

    if ((inout_generator->long_piece_count >= MAX_LONG_PIECES) || (inout_generator->cells[in_first_cell].x <= 0)
        || !prv_take_chance(inout_generator, CHANCE_EXTEND_AT_SIZE_3_OR_4))
    {
        return;
    }

    for (side = 0; side < SIDE_COUNT; ++side)
    {
        const int8_t next = prv_get_neighbour(inout_generator, in_cell, side);

        /* The original indexes straight into the neighbour here and would fail outright if
         * there were none. There can be: the mirror axis and the outer edges carry
         * connections that lead nowhere. Skipping such a side is the only place this port
         * knowingly does something the original cannot do at all. */
        if (inout_generator->cells[in_cell].connect[side] && (next >= 0)
            && prv_is_open_cell(inout_generator, next, side, NONE, NONE))
        {
            sides[count] = side;
            ++count;
        }
    }

    if (count == 0U)
    {
        return;
    }

    side = sides[(uint8_t)prv_random_int(inout_generator, 0, (int8_t)(count - 1U))];
    neighbour = prv_get_neighbour(inout_generator, in_cell, side);
    prv_connect_cell(inout_generator, neighbour, side);
    prv_fill_cell(inout_generator, prv_get_neighbour(inout_generator, neighbour, side));
    ++inout_generator->long_piece_count;
}

/* Mark which cells could be stretched taller or squeezed narrower later. Both are shapes
 * with two opposite sides open and the other two closed — a cell whose walls run one way
 * only can be moved without breaking the piece it belongs to. */
static void prv_set_resize_candidates(prv_generator_t* const inout_generator)
{
    for (int8_t index = 0; index < GRID_CELL_COUNT; ++index)
    {
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t right = prv_get_neighbour(inout_generator, index, SIDE_RIGHT);
        const bool is_left_closed = (cell->x == 0) || !cell->connect[SIDE_LEFT];
        const bool is_right_closed = (cell->x == (GRID_COLUMNS - 1)) || !cell->connect[SIDE_RIGHT];

        if (is_left_closed && is_right_closed && (cell->connect[SIDE_UP] != cell->connect[SIDE_DOWN]))
        {
            cell->can_raise_height = true;
        }

        if (right >= 0)
        {
            prv_cell_t* const other = &inout_generator->cells[right];
            const bool is_other_right_closed = (other->x == (GRID_COLUMNS - 1)) || !other->connect[SIDE_RIGHT];

            if (is_left_closed && !cell->connect[SIDE_UP] && !cell->connect[SIDE_DOWN] && is_other_right_closed
                && !other->connect[SIDE_UP] && !other->connect[SIDE_DOWN])
            {
                cell->can_raise_height = true;
                other->can_raise_height = true;
            }
        }

        if ((cell->x == (GRID_COLUMNS - 1)) && cell->connect[SIDE_RIGHT])
        {
            cell->can_shrink_width = true;
        }

        if (((cell->y == 0) || !cell->connect[SIDE_UP]) && ((cell->y == (GRID_ROWS - 1)) || !cell->connect[SIDE_DOWN])
            && (cell->connect[SIDE_LEFT] != cell->connect[SIDE_RIGHT]))
        {
            cell->can_shrink_width = true;
        }
    }
}

/* Fill the grid with pieces. */
static void prv_stack_pieces(prv_generator_t* const inout_generator)
{
    static const uint64_t k_stop_growing_chance[MAX_PIECE_SIZE + 1] = {
        CHANCE_NEVER,
        CHANCE_NEVER,
        CHANCE_STOP_GROWING_AT_SIZE_2,
        CHANCE_STOP_GROWING_AT_SIZE_3,
        CHANCE_STOP_GROWING_AT_SIZE_4,
        CHANCE_ALWAYS,
    };

    for (inout_generator->group_count = 0U;; ++inout_generator->group_count)
    {
        int8_t candidates[GRID_CELL_COUNT];
        const uint8_t candidate_count = prv_get_leftmost_empty_cells(inout_generator, candidates);
        int8_t first_cell;
        int8_t cell;
        int8_t new_cell = NONE;
        int8_t side = NONE;
        int8_t size;

        if (candidate_count == 0U)
        {
            break;
        }

        cell = candidates[(uint8_t)prv_random_int(inout_generator, 0, (int8_t)(candidate_count - 1U))];
        first_cell = cell;
        prv_fill_cell(inout_generator, cell);

        /* One single-cell piece is allowed to hang off the top edge and one off the bottom.
         * They are the little stubs the arcade's maze has, and without them every piece
         * would be at least two cells long. */
        if ((inout_generator->cells[cell].x < (GRID_COLUMNS - 1))
            && ((inout_generator->cells[cell].y == 0) || (inout_generator->cells[cell].y == (GRID_ROWS - 1)))
            && prv_take_chance(inout_generator, CHANCE_TOP_BOTTOM_SINGLE_JOIN))
        {
            const bool is_top = inout_generator->cells[cell].y == 0;
            uint8_t* const count =
                is_top ? &inout_generator->top_single_cell_count : &inout_generator->bottom_single_cell_count;

            if (*count == 0U)
            {
                inout_generator->cells[cell].connect[is_top ? SIDE_UP : SIDE_DOWN] = true;
                ++(*count);
                continue;
            }
        }

        size = 1;

        if (inout_generator->cells[cell].x == (GRID_COLUMNS - 1))
        {
            /* A piece that starts against the right edge stays a single cell: it is the
             * outer wall, and the edge is where a tunnel mouth may later be cut. */
            inout_generator->cells[cell].connect[SIDE_RIGHT] = true;
            inout_generator->cells[cell].can_raise_height = true;

            continue;
        }

        while (size < MAX_PIECE_SIZE)
        {
            bool stop = false;

            if (size == 2)
            {
                if (prv_try_extend_to_long_l(inout_generator, first_cell))
                {
                    size = (int8_t)(size + 2);
                    stop = true;
                }
            }

            if (!stop)
            {
                int8_t open_sides[SIDE_COUNT];
                uint8_t open_count = prv_get_open_sides(inout_generator, cell, side, size, open_sides);

                /* Nowhere left around the centre. At size two, try again around the cell
                 * just added instead — without that, the grid fills with two-cell stubs. */
                if ((open_count == 0U) && (size == 2))
                {
                    cell = new_cell;
                    open_count = prv_get_open_sides(inout_generator, cell, side, size, open_sides);
                }

                if (open_count == 0U)
                {
                    stop = true;
                }
                else
                {
                    side = open_sides[(uint8_t)prv_random_int(inout_generator, 0, (int8_t)(open_count - 1U))];
                    new_cell = prv_get_neighbour(inout_generator, cell, side);

                    prv_connect_cell(inout_generator, cell, side);
                    prv_fill_cell(inout_generator, new_cell);
                    ++size;

                    /* A piece on the mirror axis is drawn twice, so it counts double —
                     * three cells there is already six across the finished maze. */
                    if ((inout_generator->cells[first_cell].x == 0) && (size == 3))
                    {
                        stop = true;
                    }

                    if (prv_take_chance(inout_generator, k_stop_growing_chance[size]))
                    {
                        stop = true;
                    }
                }
            }

            if (stop)
            {
                if ((size == 3) || (size == 4))
                {
                    prv_try_extend_leg(inout_generator, first_cell, cell);
                }

                break;
            }
        }
    }

    prv_set_resize_candidates(inout_generator);
}

/* ---- choosing the row to stretch and the column to squeeze ---------------- */

/* Both searches have the same shape, and it is worth saying why they are recursive at all:
 * making one cell taller pushes its neighbours around, and the cell to the right has to be
 * able to absorb that or the corner between them becomes too tight to draw. So a choice is
 * only valid if a matching choice exists in the next column along, all the way to the edge. */
static bool prv_can_raise_height(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y)
{
    int8_t candidates[GRID_ROWS];
    uint8_t candidate_count = 0U;
    int8_t index = NONE;
    int8_t right = NONE;
    int8_t y;

    if (in_x == (GRID_COLUMNS - 1))
    {
        return true;
    }

    /* Walk up to the first cell that would not make too tight a turn on its right. */
    for (y = in_y; y >= 0; --y)
    {
        index = (int8_t)(in_x + (y * GRID_COLUMNS));
        right = prv_get_neighbour(inout_generator, index, SIDE_RIGHT);

        if ((!inout_generator->cells[index].connect[SIDE_UP] || prv_is_cross_centre(inout_generator, index))
            && (!inout_generator->cells[right].connect[SIDE_UP] || prv_is_cross_centre(inout_generator, right)))
        {
            break;
        }
    }

    /* From there, downwards, collect what could be raised. */
    for (; right >= 0; right = prv_get_neighbour(inout_generator, right, SIDE_DOWN))
    {
        const int8_t left = prv_get_neighbour(inout_generator, right, SIDE_LEFT);

        if (inout_generator->cells[right].can_raise_height)
        {
            candidates[candidate_count] = right;
            ++candidate_count;
        }

        if ((!inout_generator->cells[right].connect[SIDE_DOWN] || prv_is_cross_centre(inout_generator, right))
            && (!prv_is_connected(inout_generator, left, SIDE_DOWN) || prv_is_cross_centre(inout_generator, left)))
        {
            break;
        }
    }

    prv_shuffle(inout_generator, candidates, candidate_count);

    for (uint8_t choice = 0U; choice < candidate_count; ++choice)
    {
        const int8_t candidate = candidates[choice];

        if (prv_can_raise_height(inout_generator, inout_generator->cells[candidate].x,
                                 inout_generator->cells[candidate].y))
        {
            inout_generator->cells[candidate].raise_height = true;
            inout_generator->tall_row_of_column[inout_generator->cells[candidate].x] =
                inout_generator->cells[candidate].y;

            return true;
        }
    }

    return false;
}

static bool prv_choose_tall_rows(prv_generator_t* const inout_generator)
{
    /* From the top left downwards, but it must be found before the ghost house: the house
     * is at a fixed height and stretching a row below it would move it. */
    for (int8_t y = 0; y < 3; ++y)
    {
        const int8_t index = (int8_t)(y * GRID_COLUMNS);

        if (inout_generator->cells[index].can_raise_height && prv_can_raise_height(inout_generator, 0, y))
        {
            inout_generator->cells[index].raise_height = true;
            inout_generator->tall_row_of_column[inout_generator->cells[index].x] = inout_generator->cells[index].y;

            return true;
        }
    }

    return false;
}

static bool prv_can_shrink_width(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y)
{
    int8_t candidates[GRID_COLUMNS];
    uint8_t candidate_count = 0U;
    int8_t index = NONE;
    int8_t below = NONE;
    int8_t x;

    if (in_y == (GRID_ROWS - 1))
    {
        return true;
    }

    for (x = in_x; x < GRID_COLUMNS; ++x)
    {
        index = (int8_t)(x + (in_y * GRID_COLUMNS));
        below = prv_get_neighbour(inout_generator, index, SIDE_DOWN);

        if ((!inout_generator->cells[index].connect[SIDE_RIGHT] || prv_is_cross_centre(inout_generator, index))
            && (!inout_generator->cells[below].connect[SIDE_RIGHT] || prv_is_cross_centre(inout_generator, below)))
        {
            break;
        }
    }

    for (; below >= 0; below = prv_get_neighbour(inout_generator, below, SIDE_LEFT))
    {
        const int8_t above = prv_get_neighbour(inout_generator, below, SIDE_UP);

        if (inout_generator->cells[below].can_shrink_width)
        {
            candidates[candidate_count] = below;
            ++candidate_count;
        }

        if ((!inout_generator->cells[below].connect[SIDE_LEFT] || prv_is_cross_centre(inout_generator, below))
            && (!prv_is_connected(inout_generator, above, SIDE_LEFT) || prv_is_cross_centre(inout_generator, above)))
        {
            break;
        }
    }

    prv_shuffle(inout_generator, candidates, candidate_count);

    for (uint8_t choice = 0U; choice < candidate_count; ++choice)
    {
        const int8_t candidate = candidates[choice];

        if (prv_can_shrink_width(inout_generator, inout_generator->cells[candidate].x,
                                 inout_generator->cells[candidate].y))
        {
            inout_generator->cells[candidate].shrink_width = true;
            inout_generator->narrow_column_of_row[inout_generator->cells[candidate].y] =
                inout_generator->cells[candidate].x;

            return true;
        }
    }

    return false;
}

static bool prv_choose_narrow_columns(prv_generator_t* const inout_generator)
{
    for (int8_t x = (GRID_COLUMNS - 1); x >= 0; --x)
    {
        if (inout_generator->cells[x].can_shrink_width && prv_can_shrink_width(inout_generator, x, 0))
        {
            inout_generator->cells[x].shrink_width = true;
            inout_generator->narrow_column_of_row[inout_generator->cells[x].y] = inout_generator->cells[x].x;

            return true;
        }
    }

    return false;
}

/* ---- judging and finishing a grid ---------------------------------------- */

static void prv_replace_group(prv_generator_t* const inout_generator, uint8_t in_old_group, uint8_t in_new_group)
{
    for (int8_t index = 0; index < GRID_CELL_COUNT; ++index)
    {
        if (inout_generator->cells[index].group == in_old_group)
        {
            inout_generator->cells[index].group = in_new_group;
        }
    }
}

/* Whether a two-cell piece lies flat at this cell, and whether one stands upright. Two of
 * either, side by side, would upscale into a block big enough to look like a mistake. */
static bool prv_is_horizontal_pair(const prv_generator_t* const in_generator, int8_t in_x, int8_t in_y)
{
    const int8_t left = (int8_t)(in_x + (in_y * GRID_COLUMNS));
    const prv_cell_t* const first = &in_generator->cells[left];
    const prv_cell_t* const second = &in_generator->cells[left + 1];

    return !first->connect[SIDE_UP] && !first->connect[SIDE_DOWN] && ((in_x == 0) || !first->connect[SIDE_LEFT])
           && first->connect[SIDE_RIGHT] && !second->connect[SIDE_UP] && !second->connect[SIDE_DOWN]
           && second->connect[SIDE_LEFT] && !second->connect[SIDE_RIGHT];
}

static bool prv_is_vertical_pair(const prv_generator_t* const in_generator, int8_t in_x, int8_t in_y)
{
    const int8_t top = (int8_t)(in_x + (in_y * GRID_COLUMNS));
    const prv_cell_t* const first = &in_generator->cells[top];
    const prv_cell_t* const second = &in_generator->cells[top + GRID_COLUMNS];

    if (in_x == (GRID_COLUMNS - 1))
    {
        /* At the right edge two lone cells count as a pair, because the edge closes them. */
        return !first->connect[SIDE_LEFT] && !first->connect[SIDE_UP] && !first->connect[SIDE_DOWN]
               && !second->connect[SIDE_LEFT] && !second->connect[SIDE_UP] && !second->connect[SIDE_DOWN];
    }

    return !first->connect[SIDE_LEFT] && !first->connect[SIDE_RIGHT] && !first->connect[SIDE_UP]
           && first->connect[SIDE_DOWN] && !second->connect[SIDE_LEFT] && !second->connect[SIDE_RIGHT]
           && second->connect[SIDE_UP] && !second->connect[SIDE_DOWN];
}

/* Merge four cells that form two stacked pairs into one square piece. */
static void prv_join_into_square(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y)
{
    const int8_t top_left = (int8_t)(in_x + (in_y * GRID_COLUMNS));
    const int8_t top_right = (int8_t)(top_left + 1);
    const int8_t bottom_left = (int8_t)(top_left + GRID_COLUMNS);
    const int8_t bottom_right = (int8_t)(bottom_left + 1);
    const uint8_t group = inout_generator->cells[top_left].group;

    inout_generator->cells[top_left].connect[SIDE_DOWN] = true;
    inout_generator->cells[top_left].connect[SIDE_RIGHT] = true;

    inout_generator->cells[top_right].connect[SIDE_DOWN] = true;
    inout_generator->cells[top_right].connect[SIDE_LEFT] = true;
    inout_generator->cells[top_right].group = group;

    inout_generator->cells[bottom_left].connect[SIDE_UP] = true;
    inout_generator->cells[bottom_left].connect[SIDE_RIGHT] = true;
    inout_generator->cells[bottom_left].group = group;

    inout_generator->cells[bottom_right].connect[SIDE_UP] = true;
    inout_generator->cells[bottom_right].connect[SIDE_LEFT] = true;
    inout_generator->cells[bottom_right].group = group;
}

/* The faults the stacking has no heuristic for, caught after the fact. A grid that fails
 * here is thrown away rather than repaired — except for the stacked pairs, which have an
 * obvious repair and get it. */
static bool prv_is_desirable(prv_generator_t* const inout_generator)
{
    const prv_cell_t* const top_right = &inout_generator->cells[GRID_COLUMNS - 1];
    const prv_cell_t* const bottom_right = &inout_generator->cells[GRID_CELL_COUNT - 1];

    if (top_right->connect[SIDE_UP] || top_right->connect[SIDE_RIGHT])
    {
        return false;
    }

    if (bottom_right->connect[SIDE_DOWN] || bottom_right->connect[SIDE_RIGHT])
    {
        return false;
    }

    for (int8_t y = 0; y < (GRID_ROWS - 1); ++y)
    {
        for (int8_t x = 0; x < (GRID_COLUMNS - 1); ++x)
        {
            const bool is_stacked_flat = prv_is_horizontal_pair(inout_generator, x, y)
                                         && prv_is_horizontal_pair(inout_generator, x, (int8_t)(y + 1));
            const bool is_stacked_upright = prv_is_vertical_pair(inout_generator, x, y)
                                            && prv_is_vertical_pair(inout_generator, (int8_t)(x + 1), y);

            if (is_stacked_flat || is_stacked_upright)
            {
                /* Not on the mirror axis: joined there, the square is drawn twice and the
                 * result is a slab in the middle of the maze. */
                if (x == 0)
                {
                    return false;
                }

                prv_join_into_square(inout_generator, x, y);
            }
        }
    }

    if (!prv_choose_tall_rows(inout_generator))
    {
        return false;
    }

    return prv_choose_narrow_columns(inout_generator);
}

/* Where every cell lands once the grid is upscaled by three, with the one taller row and
 * the one narrower column applied. */
static void prv_set_upscale_coordinates(prv_generator_t* const inout_generator)
{
    for (int8_t index = 0; index < GRID_CELL_COUNT; ++index)
    {
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t narrow_column = inout_generator->narrow_column_of_row[cell->y];
        const int8_t tall_row = inout_generator->tall_row_of_column[cell->x];

        cell->final_x = (int8_t)(cell->x * 3);

        if ((narrow_column != NONE) && (narrow_column < cell->x))
        {
            --cell->final_x;
        }

        cell->final_y = (int8_t)(cell->y * 3);

        if ((tall_row != NONE) && (tall_row < cell->y))
        {
            ++cell->final_y;
        }

        cell->final_width = cell->shrink_width ? 2 : 3;
        cell->final_height = cell->raise_height ? 4 : 3;
    }
}

/* Attach some pieces to the outer boundary. Every attachment removes a way round, which is
 * the whole point: a maze of free-standing blocks is far too easy. */
static void prv_join_walls(prv_generator_t* const inout_generator)
{
    for (int8_t x = 0; x < GRID_COLUMNS; ++x)
    {
        const int8_t index = x;
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t left = prv_get_neighbour(inout_generator, index, SIDE_LEFT);
        const int8_t right = prv_get_neighbour(inout_generator, index, SIDE_RIGHT);
        const int8_t below = prv_get_neighbour(inout_generator, index, SIDE_DOWN);

        if (cell->connect[SIDE_LEFT] || cell->connect[SIDE_RIGHT] || cell->connect[SIDE_UP]
            || (cell->connect[SIDE_DOWN] && prv_is_connected(inout_generator, below, SIDE_DOWN)))
        {
            continue;
        }

        /* Not if it would seal a dead end, and not if the piece behind it is so big that
         * attaching it walls off half a row. */
        if (prv_is_connected(inout_generator, left, SIDE_UP) || (right < 0)
            || prv_is_connected(inout_generator, right, SIDE_UP))
        {
            continue;
        }

        if ((below >= 0) && inout_generator->cells[below].connect[SIDE_RIGHT]
            && prv_is_connected(inout_generator, prv_get_neighbour(inout_generator, below, SIDE_RIGHT), SIDE_RIGHT))
        {
            continue;
        }

        if (prv_take_chance(inout_generator, CHANCE_JOIN_TO_BOUNDARY))
        {
            cell->connect[SIDE_UP] = true;
        }
    }

    for (int8_t x = 0; x < GRID_COLUMNS; ++x)
    {
        const int8_t index = (int8_t)(x + ((GRID_ROWS - 1) * GRID_COLUMNS));
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t left = prv_get_neighbour(inout_generator, index, SIDE_LEFT);
        const int8_t right = prv_get_neighbour(inout_generator, index, SIDE_RIGHT);
        const int8_t above = prv_get_neighbour(inout_generator, index, SIDE_UP);

        if (cell->connect[SIDE_LEFT] || cell->connect[SIDE_RIGHT] || cell->connect[SIDE_DOWN]
            || (cell->connect[SIDE_UP] && prv_is_connected(inout_generator, above, SIDE_UP)))
        {
            continue;
        }

        if (prv_is_connected(inout_generator, left, SIDE_DOWN) || (right < 0)
            || prv_is_connected(inout_generator, right, SIDE_DOWN))
        {
            continue;
        }

        if ((above >= 0) && inout_generator->cells[above].connect[SIDE_RIGHT]
            && prv_is_connected(inout_generator, prv_get_neighbour(inout_generator, above, SIDE_RIGHT), SIDE_RIGHT))
        {
            continue;
        }

        if (prv_take_chance(inout_generator, CHANCE_JOIN_TO_BOUNDARY))
        {
            cell->connect[SIDE_DOWN] = true;
        }
    }

    for (int8_t y = 1; y < (GRID_ROWS - 1); ++y)
    {
        const int8_t index = (int8_t)((GRID_COLUMNS - 1) + (y * GRID_COLUMNS));
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t above = prv_get_neighbour(inout_generator, index, SIDE_UP);
        const int8_t below = prv_get_neighbour(inout_generator, index, SIDE_DOWN);
        int8_t left;

        if (cell->raise_height)
        {
            continue;
        }

        if (cell->connect[SIDE_RIGHT] || cell->connect[SIDE_UP] || cell->connect[SIDE_DOWN]
            || prv_is_connected(inout_generator, above, SIDE_RIGHT)
            || prv_is_connected(inout_generator, below, SIDE_RIGHT))
        {
            continue;
        }

        if (!cell->connect[SIDE_LEFT])
        {
            continue;
        }

        left = prv_get_neighbour(inout_generator, index, SIDE_LEFT);

        if (!inout_generator->cells[left].connect[SIDE_UP] && !inout_generator->cells[left].connect[SIDE_DOWN]
            && !inout_generator->cells[left].connect[SIDE_LEFT]
            && prv_take_chance(inout_generator, CHANCE_JOIN_TO_RIGHT_BOUNDARY))
        {
            cell->connect[SIDE_RIGHT] = true;
        }
    }
}

/* ---- tunnels -------------------------------------------------------------- */

typedef struct
{
    int8_t single_dead_ends[GRID_ROWS];
    int8_t top_single_dead_ends[GRID_ROWS];
    int8_t bottom_single_dead_ends[GRID_ROWS];
    int8_t voids[GRID_ROWS];
    int8_t top_voids[GRID_ROWS];
    int8_t bottom_voids[GRID_ROWS];
    int8_t edges[GRID_ROWS];
    int8_t top_edges[GRID_ROWS];
    int8_t bottom_edges[GRID_ROWS];
    int8_t double_dead_ends[GRID_ROWS];

    uint8_t single_dead_end_count;
    uint8_t top_single_dead_end_count;
    uint8_t bottom_single_dead_end_count;
    uint8_t void_count;
    uint8_t top_void_count;
    uint8_t bottom_void_count;
    uint8_t edge_count;
    uint8_t top_edge_count;
    uint8_t bottom_edge_count;
    uint8_t double_dead_end_count;
} prv_tunnel_candidates_t;

/* Which right-edge cells could carry a tunnel mouth, sorted by the kind of opening they
 * would make and by whether they are near the top or the bottom — a maze with two tunnels
 * wants one of each rather than two next to each other. */
static void prv_collect_tunnel_candidates(prv_generator_t* const inout_generator,
                                          prv_tunnel_candidates_t* const out_candidates)
{
    memset(out_candidates, 0, sizeof(*out_candidates));

    for (int8_t y = 0; y < GRID_ROWS; ++y)
    {
        const int8_t index = (int8_t)((GRID_COLUMNS - 1) + (y * GRID_COLUMNS));
        prv_cell_t* const cell = &inout_generator->cells[index];
        const int8_t above = prv_get_neighbour(inout_generator, index, SIDE_UP);
        const int8_t below = prv_get_neighbour(inout_generator, index, SIDE_DOWN);
        bool is_up_dead;
        bool is_down_dead;

        if (cell->connect[SIDE_UP])
        {
            continue;
        }

        if ((cell->y > 1) && (cell->y < (GRID_ROWS - 2)))
        {
            cell->is_edge_tunnel_candidate = true;
            out_candidates->edges[out_candidates->edge_count] = index;
            ++out_candidates->edge_count;

            if (cell->y <= 2)
            {
                out_candidates->top_edges[out_candidates->top_edge_count] = index;
                ++out_candidates->top_edge_count;
            }
            else if (cell->y >= 5)
            {
                out_candidates->bottom_edges[out_candidates->bottom_edge_count] = index;
                ++out_candidates->bottom_edge_count;
            }
            else
            {
                /* The middle rows are the ghost house's; no mouth there. */
            }
        }

        is_up_dead = (above < 0) || inout_generator->cells[above].connect[SIDE_RIGHT];
        is_down_dead = (below < 0) || inout_generator->cells[below].connect[SIDE_RIGHT];

        if (cell->connect[SIDE_RIGHT])
        {
            if (is_up_dead)
            {
                cell->is_void_tunnel_candidate = true;
                out_candidates->voids[out_candidates->void_count] = index;
                ++out_candidates->void_count;

                if (cell->y <= 2)
                {
                    out_candidates->top_voids[out_candidates->top_void_count] = index;
                    ++out_candidates->top_void_count;
                }
                else if (cell->y >= 6)
                {
                    out_candidates->bottom_voids[out_candidates->bottom_void_count] = index;
                    ++out_candidates->bottom_void_count;
                }
                else
                {
                    /* As above. */
                }
            }

            continue;
        }

        if (cell->connect[SIDE_DOWN])
        {
            continue;
        }

        if (is_up_dead != is_down_dead)
        {
            const int8_t left = prv_get_neighbour(inout_generator, index, SIDE_LEFT);

            if (!cell->raise_height && (y < (GRID_ROWS - 1)) && !inout_generator->cells[left].connect[SIDE_LEFT])
            {
                const int8_t offset = is_up_dead ? 1 : 0;

                out_candidates->single_dead_ends[out_candidates->single_dead_end_count] = index;
                ++out_candidates->single_dead_end_count;
                cell->is_single_dead_end_candidate = true;
                cell->single_dead_end_side = is_up_dead ? SIDE_UP : SIDE_DOWN;

                if (cell->y <= (1 + offset))
                {
                    out_candidates->top_single_dead_ends[out_candidates->top_single_dead_end_count] = index;
                    ++out_candidates->top_single_dead_end_count;
                }
                else if (cell->y >= (5 + offset))
                {
                    out_candidates->bottom_single_dead_ends[out_candidates->bottom_single_dead_end_count] = index;
                    ++out_candidates->bottom_single_dead_end_count;
                }
                else
                {
                    /* As above. */
                }
            }
        }
        else if (is_up_dead && is_down_dead)
        {
            const int8_t left = prv_get_neighbour(inout_generator, index, SIDE_LEFT);

            if ((y > 0) && (y < (GRID_ROWS - 1)) && inout_generator->cells[left].connect[SIDE_UP]
                && inout_generator->cells[left].connect[SIDE_DOWN])
            {
                cell->is_double_dead_end_candidate = true;

                if ((cell->y >= 2) && (cell->y <= 5))
                {
                    out_candidates->double_dead_ends[out_candidates->double_dead_end_count] = index;
                    ++out_candidates->double_dead_end_count;
                }
            }
        }
        else
        {
            /* Neither side dead: not a mouth. */
        }
    }
}

static void prv_select_single_dead_end(prv_generator_t* const inout_generator, int8_t in_index)
{
    inout_generator->cells[in_index].connect[SIDE_RIGHT] = true;

    if (inout_generator->cells[in_index].single_dead_end_side == SIDE_UP)
    {
        inout_generator->cells[in_index].is_top_tunnel = true;
    }
    else
    {
        inout_generator->cells[prv_get_neighbour(inout_generator, in_index, SIDE_DOWN)].is_top_tunnel = true;
    }
}

/* Cut one or two tunnel mouths into the right edge, then check the result is still a maze.
 * Returns `false` when it is not, which throws the whole grid away. */
static bool prv_create_tunnels(prv_generator_t* const inout_generator)
{
    prv_tunnel_candidates_t candidates;
    const int8_t wanted = prv_take_chance(inout_generator, CHANCE_TWO_TUNNELS) ? 2 : 1;
    int8_t chosen;

    prv_collect_tunnel_candidates(inout_generator, &candidates);

    if (wanted == 1)
    {
        chosen = prv_random_element(inout_generator, candidates.voids, candidates.void_count);

        if (chosen >= 0)
        {
            inout_generator->cells[chosen].is_top_tunnel = true;
        }
        else
        {
            chosen = prv_random_element(inout_generator, candidates.single_dead_ends, candidates.single_dead_end_count);

            if (chosen >= 0)
            {
                prv_select_single_dead_end(inout_generator, chosen);
            }
            else
            {
                chosen = prv_random_element(inout_generator, candidates.edges, candidates.edge_count);

                if (chosen < 0)
                {
                    return false;
                }

                inout_generator->cells[chosen].is_top_tunnel = true;
            }
        }
    }
    else
    {
        chosen = prv_random_element(inout_generator, candidates.double_dead_ends, candidates.double_dead_end_count);

        if (chosen >= 0)
        {
            /* One cell that is dead both ways gives both mouths at once. */
            inout_generator->cells[chosen].connect[SIDE_RIGHT] = true;
            inout_generator->cells[chosen].is_top_tunnel = true;
            inout_generator->cells[prv_get_neighbour(inout_generator, chosen, SIDE_DOWN)].is_top_tunnel = true;
        }
        else
        {
            uint8_t created = 1U;

            chosen = prv_random_element(inout_generator, candidates.top_voids, candidates.top_void_count);

            if (chosen >= 0)
            {
                inout_generator->cells[chosen].is_top_tunnel = true;
            }
            else
            {
                chosen = prv_random_element(inout_generator, candidates.top_single_dead_ends,
                                            candidates.top_single_dead_end_count);

                if (chosen >= 0)
                {
                    prv_select_single_dead_end(inout_generator, chosen);
                }
                else
                {
                    chosen = prv_random_element(inout_generator, candidates.top_edges, candidates.top_edge_count);

                    if (chosen >= 0)
                    {
                        inout_generator->cells[chosen].is_top_tunnel = true;
                    }
                    else
                    {
                        created = 0U;
                    }
                }
            }

            chosen = prv_random_element(inout_generator, candidates.bottom_voids, candidates.bottom_void_count);

            if (chosen >= 0)
            {
                inout_generator->cells[chosen].is_top_tunnel = true;
            }
            else
            {
                chosen = prv_random_element(inout_generator, candidates.bottom_single_dead_ends,
                                            candidates.bottom_single_dead_end_count);

                if (chosen >= 0)
                {
                    prv_select_single_dead_end(inout_generator, chosen);
                }
                else
                {
                    chosen = prv_random_element(inout_generator, candidates.bottom_edges, candidates.bottom_edge_count);

                    if (chosen >= 0)
                    {
                        inout_generator->cells[chosen].is_top_tunnel = true;
                    }
                    else if (created == 0U)
                    {
                        return false;
                    }
                    else
                    {
                        /* One mouth is enough. */
                    }
                }
            }
        }
    }

    /* A tunnel that leads straight across the maze is not a shortcut, it is a corridor with
     * no reason to leave. Reject the grid rather than patch it. */
    for (int8_t y = 0; y < GRID_ROWS; ++y)
    {
        int8_t index = (int8_t)((GRID_COLUMNS - 1) + (y * GRID_COLUMNS));

        if (inout_generator->cells[index].is_top_tunnel)
        {
            const int8_t tunnel_y = inout_generator->cells[index].final_y;
            bool is_straight_through = true;

            for (int8_t left = prv_get_neighbour(inout_generator, index, SIDE_LEFT); left >= 0;
                 left = prv_get_neighbour(inout_generator, left, SIDE_LEFT))
            {
                if (inout_generator->cells[left].connect[SIDE_UP] || (inout_generator->cells[left].final_y != tunnel_y))
                {
                    is_straight_through = false;
                    break;
                }
            }

            if (is_straight_through)
            {
                return false;
            }
        }
    }

    /* A void that was a candidate and did not become a tunnel is a dead end, so open it
     * upwards and let it belong to the piece above. */
    for (uint8_t index = 0U; index < candidates.void_count; ++index)
    {
        const int8_t cell = candidates.voids[index];
        const int8_t above = prv_get_neighbour(inout_generator, cell, SIDE_UP);

        if (!inout_generator->cells[cell].is_top_tunnel)
        {
            prv_replace_group(inout_generator, inout_generator->cells[cell].group, inout_generator->cells[above].group);
            inout_generator->cells[cell].connect[SIDE_UP] = true;
            inout_generator->cells[above].connect[SIDE_DOWN] = true;
        }
    }

    return true;
}

/* ---- turning the grid into tiles ----------------------------------------- */

/* Write a tile and its mirror image. Out-of-range writes are dropped, which the original
 * relies on. */
static void prv_set_tile(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y, char in_tile)
{
    int8_t x;

    if ((in_x < 0) || (in_x > (SUB_COLUMNS - 1)) || (in_y < 0) || (in_y > (SUB_ROWS - 1)))
    {
        return;
    }

    x = (int8_t)(in_x - 2);

    inout_generator->tiles[in_y][MIRROR_COLUMN + x] = in_tile;
    inout_generator->tiles[in_y][MIRROR_COLUMN - 1 - x] = in_tile;
}

/* The right half's tile at a position, or `'\0'` outside it — which compares equal to
 * nothing, exactly as the original's `undefined` does. */
static char prv_get_tile(const prv_generator_t* const in_generator, int8_t in_x, int8_t in_y)
{
    if ((in_x < 0) || (in_x > (SUB_COLUMNS - 1)) || (in_y < 0) || (in_y > (SUB_ROWS - 1)))
    {
        return '\0';
    }

    return in_generator->tiles[in_y][MIRROR_COLUMN + in_x - 2];
}

static void prv_set_tile_cell(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y, int8_t in_cell)
{
    if ((in_x < 0) || (in_x > (SUB_COLUMNS - 1)) || (in_y < 0) || (in_y > (SUB_ROWS - 1)))
    {
        return;
    }

    inout_generator->tile_cell[(in_x - 2) + (in_y * SUB_COLUMNS) + 2] = in_cell;
}

static int8_t prv_get_tile_cell(const prv_generator_t* const in_generator, int8_t in_x, int8_t in_y)
{
    if ((in_x < 0) || (in_x > (SUB_COLUMNS - 1)) || (in_y < 0) || (in_y > (SUB_ROWS - 1)))
    {
        return NONE;
    }

    return in_generator->tile_cell[(in_x - 2) + (in_y * SUB_COLUMNS) + 2];
}

/* The group of the cell a tile belongs to, or `GROUP_NONE` where there is no cell. */
static uint8_t prv_get_tile_group(const prv_generator_t* const in_generator, int8_t in_x, int8_t in_y)
{
    const int8_t cell = prv_get_tile_cell(in_generator, in_x, in_y);

    return (cell < 0) ? GROUP_NONE : in_generator->cells[cell].group;
}

/* Erase pellets from a tile onwards while the corridor has only one way to continue —
 * i.e. up to the next junction. This is what keeps the tunnels and the ring round the
 * ghost house free of pellets, as the arcade's are. */
static void prv_erase_until_junction(prv_generator_t* const inout_generator, int8_t in_x, int8_t in_y)
{
    int8_t x = in_x;
    int8_t y = in_y;

    for (;;)
    {
        int8_t next_x = NONE;
        int8_t next_y = NONE;
        uint8_t adjacent = 0U;

        if (prv_get_tile(inout_generator, (int8_t)(x - 1), y) == TILE_PATH)
        {
            next_x = (int8_t)(x - 1);
            next_y = y;
            ++adjacent;
        }

        if (prv_get_tile(inout_generator, (int8_t)(x + 1), y) == TILE_PATH)
        {
            next_x = (int8_t)(x + 1);
            next_y = y;
            ++adjacent;
        }

        if (prv_get_tile(inout_generator, x, (int8_t)(y - 1)) == TILE_PATH)
        {
            next_x = x;
            next_y = (int8_t)(y - 1);
            ++adjacent;
        }

        if (prv_get_tile(inout_generator, x, (int8_t)(y + 1)) == TILE_PATH)
        {
            next_x = x;
            next_y = (int8_t)(y + 1);
            ++adjacent;
        }

        if (adjacent != 1U)
        {
            break;
        }

        prv_set_tile(inout_generator, x, y, TILE_EMPTY);
        x = next_x;
        y = next_y;
    }
}

/* Where a power pellet may go, in the corridor one in from the right edge. The arcade puts
 * two on each side, near the top and the bottom, and this looks for the same places.
 *
 * Returns whether a range was found. `out_first`/`out_last` are doubled, because the
 * original's bounds can land on a half — `subrows/2` is 15.5 — and whether they do decides
 * whether a pellet is placed at all. Reproduced rather than rounded away: rounding would
 * put a pellet where the original puts none. */
static bool prv_get_top_energizer_range(const prv_generator_t* const in_generator, int16_t* const out_first,
                                        int16_t* const out_last)
{
    const int8_t x = SUB_COLUMNS - 2;
    int16_t first = NONE;
    int16_t last = SUB_ROWS; /* doubled: subrows/2 is 15.5, so 31 halves */
    int8_t y;

    for (y = 2; (y * 2) < SUB_ROWS; ++y)
    {
        if ((prv_get_tile(in_generator, x, y) == TILE_PATH)
            && (prv_get_tile(in_generator, x, (int8_t)(y + 1)) == TILE_PATH))
        {
            first = (int16_t)((y + 1) * 2);
            break;
        }
    }

    if (first == NONE)
    {
        return false;
    }

    if ((first + 14) < last)
    {
        last = (int16_t)(first + 14);
    }

    for (y = (int8_t)((first / 2) + 1); (y * 2) < last; ++y)
    {
        if (prv_get_tile(in_generator, (int8_t)(x - 1), y) == TILE_PATH)
        {
            last = (int16_t)((y - 1) * 2);
            break;
        }
    }

    *out_first = first;
    *out_last = last;

    return true;
}

static bool prv_get_bottom_energizer_range(const prv_generator_t* const in_generator, int16_t* const out_first,
                                           int16_t* const out_last)
{
    const int8_t x = SUB_COLUMNS - 2;
    int16_t first = SUB_ROWS; /* doubled, as above */
    int16_t last = NONE;
    int8_t y;

    for (y = SUB_ROWS - 3; (y * 2) >= first; --y)
    {
        if ((prv_get_tile(in_generator, x, y) == TILE_PATH)
            && (prv_get_tile(in_generator, x, (int8_t)(y + 1)) == TILE_PATH))
        {
            last = (int16_t)(y * 2);
            break;
        }
    }

    if (last == NONE)
    {
        return false;
    }

    if ((last - 14) > first)
    {
        first = (int16_t)(last - 14);
    }

    for (y = (int8_t)((last / 2) - 1); (y * 2) > first; --y)
    {
        if (prv_get_tile(in_generator, (int8_t)(x - 1), y) == TILE_PATH)
        {
            first = (int16_t)((y + 1) * 2);
            break;
        }
    }

    *out_first = first;
    *out_last = last;

    return true;
}

/* Place one power pellet in a range. The range is in halves; when it starts on a half the
 * original computes a fractional row and writes nowhere — so nothing is placed, but a
 * random number is still drawn. Both halves of that matter, so both are kept. */
static void prv_place_energizer(prv_generator_t* const inout_generator, int16_t in_first, int16_t in_last)
{
    const uint64_t span = (uint64_t)(in_last - in_first + 2);
    const uint64_t word = (uint64_t)prv_next_word(inout_generator);
    const int16_t offset = (int16_t)((word * span) >> 33);
    const int16_t row = (int16_t)(in_first + (offset * 2));

    if ((in_first % 2) != 0)
    {
        return;
    }

    prv_set_tile(inout_generator, SUB_COLUMNS - 2, (int8_t)(row / 2), TILE_ENERGIZER);
}

/* Upscale the grid, mirror it, and turn it into tiles: walls, paths, pellets, the ghost
 * house door and the two tunnel mouths. */
static void prv_build_tiles(prv_generator_t* const inout_generator)
{
    int8_t x;
    int8_t y;
    int16_t first;
    int16_t last;

    memset(inout_generator->tiles, TILE_BLANK, sizeof(inout_generator->tiles));
    memset(inout_generator->tile_cell, NONE, sizeof(inout_generator->tile_cell));

    for (int8_t index = 0; index < GRID_CELL_COUNT; ++index)
    {
        const prv_cell_t* const cell = &inout_generator->cells[index];

        for (int8_t offset_x = 0; offset_x < cell->final_width; ++offset_x)
        {
            for (int8_t offset_y = 0; offset_y < cell->final_height; ++offset_y)
            {
                prv_set_tile_cell(inout_generator, (int8_t)(cell->final_x + offset_x),
                                  (int8_t)(cell->final_y + 1 + offset_y), index);
            }
        }
    }

    /* A path tile goes wherever two different pieces meet, and along the top of the grid.
     * Everything else is decided from that. */
    for (y = 0; y < SUB_ROWS; ++y)
    {
        for (x = 0; x < SUB_COLUMNS; ++x)
        {
            const int8_t cell = prv_get_tile_cell(inout_generator, x, y);
            const int8_t left_cell = prv_get_tile_cell(inout_generator, (int8_t)(x - 1), y);
            const int8_t up_cell = prv_get_tile_cell(inout_generator, x, (int8_t)(y - 1));
            const uint8_t group = prv_get_tile_group(inout_generator, x, y);

            if (cell >= 0)
            {
                const bool is_vertical_boundary =
                    (left_cell >= 0) && (group != prv_get_tile_group(inout_generator, (int8_t)(x - 1), y));
                const bool is_horizontal_boundary =
                    (up_cell >= 0) && (group != prv_get_tile_group(inout_generator, x, (int8_t)(y - 1)));

                if (is_vertical_boundary || is_horizontal_boundary
                    || ((up_cell < 0) && !inout_generator->cells[cell].connect[SIDE_UP]))
                {
                    prv_set_tile(inout_generator, x, y, TILE_PATH);
                }
            }
            else
            {
                const bool is_right_boundary = (left_cell >= 0)
                                               && (!inout_generator->cells[left_cell].connect[SIDE_RIGHT]
                                                   || (prv_get_tile(inout_generator, (int8_t)(x - 1), y) == TILE_PATH));
                const bool is_bottom_boundary =
                    (up_cell >= 0)
                    && (!inout_generator->cells[up_cell].connect[SIDE_DOWN]
                        || (prv_get_tile(inout_generator, x, (int8_t)(y - 1)) == TILE_PATH));

                if (is_right_boundary || is_bottom_boundary)
                {
                    prv_set_tile(inout_generator, x, y, TILE_PATH);
                }
            }

            /* Where two paths meet diagonally, the corner between them is a path too, or
             * the corridor would be pinched shut. */
            if ((prv_get_tile(inout_generator, (int8_t)(x - 1), y) == TILE_PATH)
                && (prv_get_tile(inout_generator, x, (int8_t)(y - 1)) == TILE_PATH)
                && (prv_get_tile(inout_generator, (int8_t)(x - 1), (int8_t)(y - 1)) == TILE_BLANK))
            {
                prv_set_tile(inout_generator, x, y, TILE_PATH);
            }
        }
    }

    /* Carry the chosen mouths out through the edge. */
    for (int8_t index = GRID_COLUMNS - 1; index < GRID_CELL_COUNT; index = (int8_t)(index + GRID_COLUMNS))
    {
        if (inout_generator->cells[index].is_top_tunnel)
        {
            y = (int8_t)(inout_generator->cells[index].final_y + 1);
            prv_set_tile(inout_generator, SUB_COLUMNS - 1, y, TILE_PATH);
            prv_set_tile(inout_generator, SUB_COLUMNS - 2, y, TILE_PATH);
        }
    }

    /* Any blank that touches a path, corners included, is the wall you can see. */
    for (y = 0; y < SUB_ROWS; ++y)
    {
        for (x = 0; x < SUB_COLUMNS; ++x)
        {
            static const int8_t k_offsets[8][2] = {
                {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1},
            };

            if (prv_get_tile(inout_generator, x, y) == TILE_PATH)
            {
                continue;
            }

            for (uint8_t offset = 0U; offset < 8U; ++offset)
            {
                if (prv_get_tile(inout_generator, (int8_t)(x + k_offsets[offset][0]),
                                 (int8_t)(y + k_offsets[offset][1]))
                    == TILE_PATH)
                {
                    prv_set_tile(inout_generator, x, y, TILE_WALL);
                    break;
                }
            }
        }
    }

    prv_set_tile(inout_generator, 2, HOUSE_GATE_ROW, TILE_DOOR);

    if (prv_get_top_energizer_range(inout_generator, &first, &last))
    {
        prv_place_energizer(inout_generator, first, last);
    }

    if (prv_get_bottom_energizer_range(inout_generator, &first, &last))
    {
        prv_place_energizer(inout_generator, first, last);
    }

    /* No pellets in the tunnels — a tunnel is where a ghost is slow, not where a player is
     * rewarded for going. */
    for (y = 0; y < SUB_ROWS; ++y)
    {
        if (prv_get_tile(inout_generator, SUB_COLUMNS - 1, y) == TILE_PATH)
        {
            prv_erase_until_junction(inout_generator, (int8_t)(SUB_COLUMNS - 1), y);
        }
    }

    prv_set_tile(inout_generator, 1, SUB_ROWS - 8, TILE_EMPTY);

    /* And none in the ring around the ghost house, above, below and beside it. */
    for (int8_t offset = 0; offset < 7; ++offset)
    {
        int8_t step;

        y = SUB_ROWS - 14;
        prv_set_tile(inout_generator, offset, y, TILE_EMPTY);

        for (step = 1; (prv_get_tile(inout_generator, offset, (int8_t)(y + step)) == TILE_PATH)
                       && (prv_get_tile(inout_generator, (int8_t)(offset - 1), (int8_t)(y + step)) == TILE_WALL)
                       && (prv_get_tile(inout_generator, (int8_t)(offset + 1), (int8_t)(y + step)) == TILE_WALL);
             ++step)
        {
            prv_set_tile(inout_generator, offset, (int8_t)(y + step), TILE_EMPTY);
        }

        y = SUB_ROWS - 20;
        prv_set_tile(inout_generator, offset, y, TILE_EMPTY);

        for (step = 1; (prv_get_tile(inout_generator, offset, (int8_t)(y - step)) == TILE_PATH)
                       && (prv_get_tile(inout_generator, (int8_t)(offset - 1), (int8_t)(y - step)) == TILE_WALL)
                       && (prv_get_tile(inout_generator, (int8_t)(offset + 1), (int8_t)(y - step)) == TILE_WALL);
             ++step)
        {
            prv_set_tile(inout_generator, offset, (int8_t)(y - step), TILE_EMPTY);
        }
    }

    for (int8_t offset = 0; offset < 7; ++offset)
    {
        int8_t step;

        x = 6;
        y = (int8_t)(SUB_ROWS - 14 - offset);
        prv_set_tile(inout_generator, x, y, TILE_EMPTY);

        for (step = 1; (prv_get_tile(inout_generator, (int8_t)(x + step), y) == TILE_PATH)
                       && (prv_get_tile(inout_generator, (int8_t)(x + step), (int8_t)(y - 1)) == TILE_WALL)
                       && (prv_get_tile(inout_generator, (int8_t)(x + step), (int8_t)(y + 1)) == TILE_WALL);
             ++step)
        {
            prv_set_tile(inout_generator, (int8_t)(x + step), y, TILE_EMPTY);
        }
    }
}

/* ---- the playfield map --------------------------------------------------- */

/* Translate the generator's tiles into the legend `playfield` loads, then stamp in the
 * things the game needs at a known place. */
static void prv_write_map(const prv_generator_t* const in_generator, playfield_map_t* const out_map)
{
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const char tile = in_generator->tiles[y][x];
            char mapped;

            switch (tile)
            {
                case TILE_PATH: mapped = PLAYFIELD_MAP_PELLET; break;
                case TILE_ENERGIZER: mapped = PLAYFIELD_MAP_POWER_PELLET; break;
                case TILE_EMPTY: mapped = PLAYFIELD_MAP_OPEN; break;
                case TILE_DOOR: mapped = PLAYFIELD_MAP_GATE; break;
                default: mapped = PLAYFIELD_MAP_WALL; break;
            }

            out_map->rows[y][x] = mapped;
        }

        out_map->rows[y][PLAYFIELD_WIDTH] = '\0';
    }

    /* The ghost house's inside, which is a place rather than a wall: the generator leaves it
     * blank, because to it the house is one more piece of wall. */
    for (int16_t y = HOUSE_FIRST_INTERIOR_ROW; y <= HOUSE_LAST_INTERIOR_ROW; ++y)
    {
        for (int16_t x = HOUSE_FIRST_INTERIOR_COLUMN; x <= HOUSE_LAST_INTERIOR_COLUMN; ++x)
        {
            out_map->rows[y][x] = PLAYFIELD_MAP_HOUSE;
        }
    }

    out_map->rows[HOUSE_GHOST_ROW][INKY_START_COLUMN] = PLAYFIELD_MAP_GHOST_START_FIRST + 2;
    out_map->rows[HOUSE_GHOST_ROW][PINKY_START_COLUMN] = PLAYFIELD_MAP_GHOST_START_FIRST + 1;
    out_map->rows[HOUSE_GHOST_ROW][CLYDE_START_COLUMN] = PLAYFIELD_MAP_GHOST_START_FIRST + 3;
    out_map->rows[BLINKY_START_ROW][BLINKY_START_COLUMN] = PLAYFIELD_MAP_GHOST_START_FIRST;
    out_map->rows[PACMAN_START_ROW][PACMAN_START_COLUMN] = PLAYFIELD_MAP_PACMAN_START;

    /* The tunnels: the pellet-free run in from each edge, which is exactly the stretch the
     * generator cleared and exactly what a ghost has to crawl through (§10.9). Shorter than
     * the arcade's six cells, because a generated maze has no room for its side masses. */
    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        int16_t x;

        if ((out_map->rows[y][0] != PLAYFIELD_MAP_OPEN)
            || (out_map->rows[y][PLAYFIELD_WIDTH - 1] != PLAYFIELD_MAP_OPEN))
        {
            continue;
        }

        for (x = 0; (x < PLAYFIELD_WIDTH) && (out_map->rows[y][x] == PLAYFIELD_MAP_OPEN); ++x)
        {
            out_map->rows[y][x] = PLAYFIELD_MAP_TUNNEL;
        }

        for (x = PLAYFIELD_WIDTH - 1; (x >= 0) && (out_map->rows[y][x] == PLAYFIELD_MAP_OPEN); --x)
        {
            out_map->rows[y][x] = PLAYFIELD_MAP_TUNNEL;
        }
    }
}

/* Build grids until one is worth keeping.
 *
 * `tall_row_of_column` and `narrow_column_of_row` are **not** cleared between attempts, and
 * that is the original's behaviour rather than an oversight of the port: a rejected grid
 * leaves its choices behind and the next attempt inherits them. Clearing them here would
 * make this a different generator, so they are cleared once per seed instead — which is
 * what makes one seed mean one maze. */
static void prv_generate_grid(prv_generator_t* const inout_generator)
{
    uint16_t attempt;

    memset(inout_generator->tall_row_of_column, NONE, sizeof(inout_generator->tall_row_of_column));
    memset(inout_generator->narrow_column_of_row, NONE, sizeof(inout_generator->narrow_column_of_row));

    for (attempt = 0U; attempt < MAX_ATTEMPTS; ++attempt)
    {
        prv_reset(inout_generator);
        prv_stack_pieces(inout_generator);

        if (!prv_is_desirable(inout_generator))
        {
            continue;
        }

        prv_set_upscale_coordinates(inout_generator);
        prv_join_walls(inout_generator);

        if (prv_create_tunnels(inout_generator))
        {
            break;
        }
    }

    /* Never seen: the worst of 2000 seeds needed 24 attempts. Reaching the ceiling means
     * the generator can no longer satisfy its own rules, which is a defect, not bad luck. */
    ASSERT(attempt < MAX_ATTEMPTS);
}

/* ==========================================================================
 * maze_gen - public
 * ========================================================================= */

void maze_gen_generate(playfield_map_t* out_map, uint32_t in_seed)
{
    prv_generator_t generator;

    ASSERT(out_map != NULL);

    memset(&generator, 0, sizeof(generator));

    /* xorshift32 dies at zero, and a seed of zero has to keep working — a game that starts
     * its run counter at zero would otherwise get one maze for ever. */
    generator.random_state = (in_seed != 0U) ? in_seed : 0x1234567U;

    prv_generate_grid(&generator);
    prv_build_tiles(&generator);
    prv_write_map(&generator, out_map);
}
