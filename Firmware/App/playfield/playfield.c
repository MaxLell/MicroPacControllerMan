#include "playfield.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "msg.h"

/* ==========================================================================
 * playfield - private
 * ========================================================================= */

/* Map legend, as in [10 §10.2]: '#' wall, '.' pellet, 'o' power pellet, 'P' Pacman start,
 * 'H' inside the ghost house, 'D' its gate, 'T' tunnel, ' ' open with nothing on it.
 *
 * '0'..'3' are the four ghosts' starting cells, numbered as `ghost_personality_e` numbers
 * them — Blinky, Pinky, Inky, Clyde. Blinky's is the one *outside*, just above the gate,
 * because that is where the arcade puts him; the other three stand inside. Digits rather
 * than initials because 'P' is already Pacman's and 'C'/'I' would read as maze pieces. */
#define MAP_WALL             '#'
#define MAP_PELLET           '.'
#define MAP_POWER_PELLET     'o'
#define MAP_PACMAN_START     'P'
#define MAP_HOUSE            'H'
#define MAP_GATE             'D'
#define MAP_TUNNEL           'T'
#define MAP_OPEN             ' '

#define SCATTER_CORNER_COUNT (4U)

/* The maze — **28 x 31 cells, as FR-022 has always said**, in the arcade's proportions.
 *
 * It used to be an 11 x 9 reduction, and that was right at the time: 28 columns of 8 px
 * is 224, and the panel this project started on was 128 px wide. That panel is gone, and
 * a 240 x 320 one holds the arcade layout at its native 8 px per cell with room to spare.
 * The reduction outlived its reason by one hardware change.
 *
 * **One maze, every level** (FR-025 as re-baselined). The arcade never changed its maze
 * either; difficulty comes from the ghosts getting faster, Cruise Elroy waking up and the
 * frightened window shrinking (§10.9), which is where it belongs — those live in
 * `difficulty` and the game rules, not here.
 *
 * It is **the arcade's layout, not a likeness of it**, and it did not start that way: it
 * was hand-drawn to the right proportions first, and 94 of its 868 cells were wrong. What
 * settled it was needing the arcade's own wall tiles to draw with (`game_view`), because a
 * tile map only fits the maze it was drawn for. Both now come from the same source, so the
 * picture and the rules cannot disagree about where a wall is.
 *
 * That also makes the pellet count **exactly the arcade's 244**, which matters: §10.9's
 * Cruise Elroy thresholds are absolute counts against that number. They used to transfer
 * on the strength of a coincidence — the hand-drawn maze happened to hold 243.
 * #playfield_get_total_pellet_count and a unit test keep it that way.
 *
 * Its properties are checked mechanically by the unit tests rather than by eye:
 * left-right symmetric, fully connected *through the tunnel*, a central pen with one
 * cell per ghost. A single mistyped wall can seal off a region, and that would otherwise
 * only turn up as an unreachable pellet halfway through a game.
 *
 * The formatter is turned off across the table on purpose: one row per line *is* the
 * maze. Reflowed to fill 120 columns the layout stops being readable, and a mistyped
 * wall — the one mistake this table actually suffers from — becomes invisible. */
/* clang-format off */
static const char* const g_maze[PLAYFIELD_HEIGHT] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "######.##### ## #####.######",
    "######.##     0    ##.######",
    "######.## ###DD### ##.######",
    "######.## #HHHHHH# ##.######",
    "TTTTTT.   #2H1HH3#   .TTTTTT",
    "######.## #HHHHHH# ##.######",
    "######.## ######## ##.######",
    "######.##          ##.######",
    "######.## ######## ##.######",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......P .......##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
};
/* clang-format on */

/* Positive modulo, so a coordinate one step off the low edge lands on the high one. */
static int16_t prv_wrap_coordinate(int16_t in_value, int16_t in_size)
{
    int16_t wrapped = (int16_t)(in_value % in_size);

    if (wrapped < 0)
    {
        wrapped = (int16_t)(wrapped + in_size);
    }

    return wrapped;
}

/* ==========================================================================
 * playfield - public
 * ========================================================================= */

void playfield_load(playfield_t* inout_playfield)
{
    uint8_t start_count = 0U;

    ASSERT(inout_playfield != NULL);

    inout_playfield->remaining_pellet_count = 0U;
    inout_playfield->pacman_start.x = 0;
    inout_playfield->pacman_start.y = 0;

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        const char* const row = g_maze[y];

        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const char tile = row[x];
            const cell_t cell = {x, y};

            inout_playfield->walls[y][x] = (tile == MAP_WALL);
            inout_playfield->tunnels[y][x] = (tile == MAP_TUNNEL);
            inout_playfield->gates[y][x] = (tile == MAP_GATE);
            inout_playfield->pellets[y][x] = PLAYFIELD_PELLET_NONE;

            /* The gate counts as house, so a ghost standing on it is still on its way in or
             * out rather than already loose in the maze. Blinky's cell does not: he starts
             * outside, and the digit that marks him sits above the gate. */
            inout_playfield->house[y][x] =
                (tile == MAP_HOUSE) || (tile == MAP_GATE) || ((tile >= '1') && (tile <= '3'));

            if (tile == MAP_PELLET)
            {
                inout_playfield->pellets[y][x] = PLAYFIELD_PELLET_NORMAL;
                ++inout_playfield->remaining_pellet_count;
            }
            else if (tile == MAP_POWER_PELLET)
            {
                inout_playfield->pellets[y][x] = PLAYFIELD_PELLET_POWER;
                ++inout_playfield->remaining_pellet_count;
            }
            else if (tile == MAP_PACMAN_START)
            {
                inout_playfield->pacman_start = cell;
            }
            else if ((tile >= '0') && (tile < (char)('0' + PLAYFIELD_GHOST_COUNT)))
            {
                inout_playfield->ghost_starts[tile - '0'] = cell;
                ++start_count;
            }
            else
            {
                /* A wall or a bare open cell — nothing to record. */
            }
        }
    }

    /* A maze missing a start, or with two ghosts on one digit, is a typo in the table
     * above rather than a runtime condition. */
    ASSERT(start_count == PLAYFIELD_GHOST_COUNT);
    ASSERT(inout_playfield->remaining_pellet_count > 0U);

    inout_playfield->total_pellet_count = inout_playfield->remaining_pellet_count;
}

cell_t playfield_wrap_cell(cell_t in_cell)
{
    cell_t wrapped;

    wrapped.x = prv_wrap_coordinate(in_cell.x, PLAYFIELD_WIDTH);
    wrapped.y = prv_wrap_coordinate(in_cell.y, PLAYFIELD_HEIGHT);

    return wrapped;
}

bool playfield_is_walkable(const playfield_t* in_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    ASSERT(in_playfield != NULL);

    return !in_playfield->walls[cell.y][cell.x];
}

bool playfield_is_tunnel(const playfield_t* in_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    ASSERT(in_playfield != NULL);

    return in_playfield->tunnels[cell.y][cell.x];
}

playfield_pellet_e playfield_get_pellet(const playfield_t* in_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    ASSERT(in_playfield != NULL);

    return in_playfield->pellets[cell.y][cell.x];
}

playfield_pellet_e playfield_eat_pellet(playfield_t* inout_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);
    playfield_pellet_e eaten;

    ASSERT(inout_playfield != NULL);

    eaten = inout_playfield->pellets[cell.y][cell.x];

    if (eaten == PLAYFIELD_PELLET_NONE)
    {
        return PLAYFIELD_PELLET_NONE;
    }

    inout_playfield->pellets[cell.y][cell.x] = PLAYFIELD_PELLET_NONE;

    ASSERT(inout_playfield->remaining_pellet_count > 0U);
    --inout_playfield->remaining_pellet_count;

    return eaten;
}

uint16_t playfield_get_remaining_pellet_count(const playfield_t* in_playfield)
{
    ASSERT(in_playfield != NULL);

    return in_playfield->remaining_pellet_count;
}

uint16_t playfield_get_total_pellet_count(const playfield_t* in_playfield)
{
    ASSERT(in_playfield != NULL);

    return in_playfield->total_pellet_count;
}

bool playfield_is_cleared(const playfield_t* in_playfield)
{
    return playfield_get_remaining_pellet_count(in_playfield) == 0U;
}

cell_t playfield_get_pacman_start(const playfield_t* in_playfield)
{
    ASSERT(in_playfield != NULL);

    return in_playfield->pacman_start;
}

cell_t playfield_get_ghost_start(const playfield_t* in_playfield, uint8_t in_index)
{
    ASSERT(in_playfield != NULL);
    ASSERT(in_index < PLAYFIELD_GHOST_COUNT);

    return in_playfield->ghost_starts[in_index];
}

cell_t playfield_get_house_exit(const playfield_t* in_playfield)
{
    ASSERT(in_playfield != NULL);

    /* Blinky's starting cell, and that is not a coincidence: the arcade stands him on the
     * very tile the other three emerge onto, which is why the map needs no separate mark
     * for it. */
    return in_playfield->ghost_starts[0];
}

bool playfield_is_house(const playfield_t* in_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    ASSERT(in_playfield != NULL);

    return in_playfield->house[cell.y][cell.x];
}

bool playfield_is_gate(const playfield_t* in_playfield, cell_t in_cell)
{
    const cell_t cell = playfield_wrap_cell(in_cell);

    ASSERT(in_playfield != NULL);

    return in_playfield->gates[cell.y][cell.x];
}

cell_t playfield_get_scatter_corner(uint8_t in_index)
{
    /* Just inside the outer wall, so the corner is a cell a ghost can actually reach
     * rather than one it can only aim at. */
    static const cell_t k_corners[SCATTER_CORNER_COUNT] = {
        {1, 1}, {PLAYFIELD_WIDTH - 2, 1}, {1, PLAYFIELD_HEIGHT - 2}, {PLAYFIELD_WIDTH - 2, PLAYFIELD_HEIGHT - 2}};

    ASSERT(in_index < SCATTER_CORNER_COUNT);

    return k_corners[in_index];
}

cell_t playfield_step(cell_t in_cell, direction_e in_direction)
{
    cell_t stepped = in_cell;

    switch (in_direction)
    {
        case DIRECTION_NORTH: stepped.y = (int16_t)(stepped.y - 1); break;
        case DIRECTION_SOUTH: stepped.y = (int16_t)(stepped.y + 1); break;
        case DIRECTION_WEST: stepped.x = (int16_t)(stepped.x - 1); break;
        case DIRECTION_EAST: stepped.x = (int16_t)(stepped.x + 1); break;
        case DIRECTION_NONE: break;
        default: ASSERT(false); break;
    }

    return playfield_wrap_cell(stepped);
}

uint32_t playfield_get_squared_distance(cell_t in_first, cell_t in_second)
{
    const int32_t delta_x = (int32_t)in_first.x - (int32_t)in_second.x;
    const int32_t delta_y = (int32_t)in_first.y - (int32_t)in_second.y;

    return (uint32_t)((delta_x * delta_x) + (delta_y * delta_y));
}

bool playfield_are_cells_equal(cell_t in_first, cell_t in_second)
{
    return (in_first.x == in_second.x) && (in_first.y == in_second.y);
}

direction_e playfield_get_opposite_direction(direction_e in_direction)
{
    switch (in_direction)
    {
        case DIRECTION_NORTH: return DIRECTION_SOUTH;
        case DIRECTION_SOUTH: return DIRECTION_NORTH;
        case DIRECTION_WEST: return DIRECTION_EAST;
        case DIRECTION_EAST: return DIRECTION_WEST;
        case DIRECTION_NONE: return DIRECTION_NONE;
        default: ASSERT(false); break;
    }

    return DIRECTION_NONE;
}
