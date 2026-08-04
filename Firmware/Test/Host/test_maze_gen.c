/*
 * test_maze_gen.c
 *
 * What a generated maze has to be true of.
 *
 * A generated maze cannot be checked by looking at it — there is no maze to look at, only a
 * rule that makes them. So the tests here are the properties the rest of the game depends on,
 * checked over many seeds: that it is symmetric, that every pellet can be reached, that the
 * ghost house is where the spawn and release logic expects it, and that one seed always gives
 * one maze. A generator that broke any of them would produce a maze that *looks* fine and is
 * unplayable — a sealed-off pocket of pellets that stops a level ever clearing.
 *
 * The port's *faithfulness* to shaunlebron's original is not checked here; that was done by
 * running both and comparing the output byte for byte over 300 seeds, which needs node and
 * belongs outside the unit suite. See [M4 Random Mazes](../../../Docu/Design/M4-Random-Mazes.md).
 */
#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "maze_gen.h"
#include "playfield.h"

/* Enough seeds that a rule broken in one corner of the generator shows up, few enough that
 * the suite stays quick: generating one maze is a few hundred microseconds. */
#define SEED_COUNT             (100U)

/* Where the game requires the fixed furniture to be, independently of `maze_gen`'s own
 * constants — repeating them here on purpose. A test that imported them could not catch the
 * generator moving the ghost house, which is the thing that would break ghost release,
 * revival and the rule that Pacman may never enter. */
#define GATE_ROW               (12)
#define GATE_LEFT_COLUMN       (13)
#define GATE_RIGHT_COLUMN      (14)
#define HOUSE_FIRST_ROW        (13)
#define HOUSE_LAST_ROW         (15)
#define HOUSE_FIRST_COLUMN     (11)
#define HOUSE_LAST_COLUMN      (16)
#define PACMAN_START_COLUMN    (13)
#define PACMAN_START_ROW       (23)
#define BLINKY_START_COLUMN    (14)
#define BLINKY_START_ROW       (11)

/* The arcade holds 244 pellets and four power pellets. A generated maze is a different shape,
 * so the count moves; what matters is that it stays in a band where the game still works —
 * Cruise Elroy's thresholds are counted against the total (§10.9), and a maze with almost
 * nothing to eat would clear before a ghost ever left the house. Measured over 2000 seeds the
 * range is 220..282. */
#define MIN_SENSIBLE_PELLETS   (180U)
#define MAX_SENSIBLE_PELLETS   (320U)
#define EXPECTED_POWER_PELLETS (4U)

void setUp(void)
{
    assert_probe_begin();
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * helpers
 * ========================================================================= */

static bool prv_is_walkable(const playfield_map_t* const in_map, int16_t in_x, int16_t in_y)
{
    return in_map->rows[in_y][in_x] != PLAYFIELD_MAP_WALL;
}

/* Every cell reachable on foot from Pacman's start, the edge wrap included — which is the only
 * way the two halves of a maze with a tunnel are connected at all. */
static uint16_t prv_flood_fill(const playfield_map_t* const in_map, bool out_seen[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH])
{
    /* A worklist rather than recursion: 868 cells of stack is not something to spend on a
     * host test that also has to run on nothing in particular. */
    static cell_t queue[PLAYFIELD_HEIGHT * PLAYFIELD_WIDTH];
    uint16_t head = 0U;
    uint16_t tail = 0U;
    uint16_t count = 0U;

    memset(out_seen, 0, sizeof(bool) * PLAYFIELD_HEIGHT * PLAYFIELD_WIDTH);

    queue[tail].x = PACMAN_START_COLUMN;
    queue[tail].y = PACMAN_START_ROW;
    ++tail;
    out_seen[PACMAN_START_ROW][PACMAN_START_COLUMN] = true;
    ++count;

    while (head < tail)
    {
        static const direction_e k_directions[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};
        const cell_t cell = queue[head];

        ++head;

        for (uint8_t index = 0U; index < (sizeof(k_directions) / sizeof(k_directions[0])); ++index)
        {
            const cell_t next = playfield_step(cell, k_directions[index]);

            if (out_seen[next.y][next.x] || !prv_is_walkable(in_map, next.x, next.y))
            {
                continue;
            }

            out_seen[next.y][next.x] = true;
            ++count;
            queue[tail] = next;
            ++tail;
        }
    }

    return count;
}

/* ==========================================================================
 * one seed, one maze
 * ========================================================================= */

void test_the_same_seed_gives_the_same_maze(void)
{
    /* The whole point of a seed. Without it a maze could not be reported, reproduced or
     * regression-tested — a player saying "the maze in level 4 had a hole in it" would be
     * unanswerable. */
    playfield_map_t first;
    playfield_map_t second;

    maze_gen_generate(&first, 987654U);
    maze_gen_generate(&second, 987654U);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        TEST_ASSERT_EQUAL_STRING(first.rows[row], second.rows[row]);
    }
}

void test_different_seeds_give_different_mazes(void)
{
    playfield_map_t previous;
    playfield_map_t current;

    maze_gen_generate(&previous, 1U);

    for (uint32_t seed = 2U; seed <= 12U; ++seed)
    {
        bool differs = false;

        maze_gen_generate(&current, seed);

        for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
        {
            if (strcmp(previous.rows[row], current.rows[row]) != 0)
            {
                differs = true;
            }
        }

        TEST_ASSERT_TRUE_MESSAGE(differs, "two seeds in a row produced the same maze");

        previous = current;
    }
}

void test_a_seed_of_zero_still_gives_a_maze(void)
{
    /* xorshift32 is stuck at zero, so the generator has to substitute — and a caller counting
     * runs from zero would otherwise get the same maze for ever. */
    playfield_map_t map;
    playfield_t rules;

    maze_gen_generate(&map, 0U);
    playfield_load_from_map(&rules, &map);

    TEST_ASSERT_GREATER_THAN_UINT16(MIN_SENSIBLE_PELLETS, playfield_get_total_pellet_count(&rules));
}

/* ==========================================================================
 * every maze, over many seeds
 * ========================================================================= */

void test_every_maze_is_left_right_symmetric(void)
{
    /* Not decoration: it is what makes a generated maze read as Pacman's rather than as a
     * maze, and it falls out of generating only half of one. */
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;

        maze_gen_generate(&map, seed);

        for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
        {
            for (uint8_t column = 0U; column < (PLAYFIELD_WIDTH / 2U); ++column)
            {
                const char left = map.rows[row][column];
                const char right = map.rows[row][PLAYFIELD_WIDTH - 1U - column];
                char message[80];

                /* The furniture is stamped on one side only — Pacman's start, the ghosts' —
                 * so only the maze itself is compared. */
                if ((left == PLAYFIELD_MAP_PACMAN_START) || (right == PLAYFIELD_MAP_PACMAN_START)
                    || ((left >= PLAYFIELD_MAP_GHOST_START_FIRST) && (left <= '3'))
                    || ((right >= PLAYFIELD_MAP_GHOST_START_FIRST) && (right <= '3')))
                {
                    continue;
                }

                (void)snprintf(message, sizeof(message), "seed %u: row %u is not symmetric", (unsigned)seed, row);
                TEST_ASSERT_EQUAL_CHAR_MESSAGE(left, right, message);
            }
        }
    }
}

void test_every_pellet_can_be_reached(void)
{
    /* The failure this exists for: a pocket of pellets walled off from the rest. The level
     * would never clear, and the run would sit there for ever with nothing left to do. */
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        bool seen[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
        uint16_t walkable = 0U;
        uint16_t reached;

        maze_gen_generate(&map, seed);
        reached = prv_flood_fill(&map, seen);

        for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
        {
            for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
            {
                char message[96];

                if (!prv_is_walkable(&map, (int16_t)column, (int16_t)row))
                {
                    continue;
                }

                ++walkable;

                (void)snprintf(message, sizeof(message), "seed %u: cell %u,%u is walled off", (unsigned)seed, column,
                               row);
                TEST_ASSERT_TRUE_MESSAGE(seen[row][column], message);
            }
        }

        TEST_ASSERT_EQUAL_UINT16(walkable, reached);
    }
}

void test_every_maze_has_the_ghost_house_where_the_game_expects_it(void)
{
    /* Ghost release, revival after being eaten, the gate Pacman may never cross and the
     * scatter targets are all described against these coordinates (§10.4, §10.5). The
     * generator's own grid happens to put its house here; if that ever stopped being true the
     * ghosts would spawn inside a wall, and this is the line that would say so. */
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        playfield_t rules;
        char message[80];

        maze_gen_generate(&map, seed);
        playfield_load_from_map(&rules, &map);

        (void)snprintf(message, sizeof(message), "seed %u", (unsigned)seed);

        TEST_ASSERT_EQUAL_CHAR_MESSAGE(PLAYFIELD_MAP_GATE, map.rows[GATE_ROW][GATE_LEFT_COLUMN], message);
        TEST_ASSERT_EQUAL_CHAR_MESSAGE(PLAYFIELD_MAP_GATE, map.rows[GATE_ROW][GATE_RIGHT_COLUMN], message);

        for (int16_t row = HOUSE_FIRST_ROW; row <= HOUSE_LAST_ROW; ++row)
        {
            for (int16_t column = HOUSE_FIRST_COLUMN; column <= HOUSE_LAST_COLUMN; ++column)
            {
                const cell_t cell = {column, row};

                TEST_ASSERT_TRUE_MESSAGE(playfield_is_house(&rules, cell), message);
                TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(&rules, cell), message);
            }
        }

        /* Blinky waits outside, on the cell the other three come out onto. */
        TEST_ASSERT_EQUAL_INT16(BLINKY_START_COLUMN, playfield_get_house_exit(&rules).x);
        TEST_ASSERT_EQUAL_INT16(BLINKY_START_ROW, playfield_get_house_exit(&rules).y);
        TEST_ASSERT_FALSE_MESSAGE(playfield_is_house(&rules, playfield_get_house_exit(&rules)), message);

        for (uint8_t index = 1U; index < PLAYFIELD_GHOST_COUNT; ++index)
        {
            TEST_ASSERT_TRUE_MESSAGE(playfield_is_house(&rules, playfield_get_ghost_start(&rules, index)), message);
        }
    }
}

void test_every_maze_starts_pacman_in_the_open_with_nothing_to_eat_underfoot(void)
{
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        playfield_t rules;
        const cell_t start = {PACMAN_START_COLUMN, PACMAN_START_ROW};
        char message[80];

        maze_gen_generate(&map, seed);
        playfield_load_from_map(&rules, &map);

        (void)snprintf(message, sizeof(message), "seed %u", (unsigned)seed);

        TEST_ASSERT_EQUAL_INT16(PACMAN_START_COLUMN, playfield_get_pacman_start(&rules).x);
        TEST_ASSERT_EQUAL_INT16(PACMAN_START_ROW, playfield_get_pacman_start(&rules).y);
        TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(&rules, start), message);
        TEST_ASSERT_FALSE_MESSAGE(playfield_is_house(&rules, start), message);

        /* Standing on a pellet at the start would score a point before the player moved. */
        TEST_ASSERT_EQUAL_UINT(PLAYFIELD_PELLET_NONE, playfield_get_pellet(&rules, start));
    }
}

void test_every_maze_holds_a_playable_number_of_pellets(void)
{
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        playfield_t rules;
        uint16_t power_pellets = 0U;
        char message[96];

        maze_gen_generate(&map, seed);
        playfield_load_from_map(&rules, &map);

        for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
        {
            for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
            {
                if (map.rows[row][column] == PLAYFIELD_MAP_POWER_PELLET)
                {
                    ++power_pellets;
                }
            }
        }

        (void)snprintf(message, sizeof(message), "seed %u: %u pellets, %u power", (unsigned)seed,
                       playfield_get_total_pellet_count(&rules), power_pellets);

        TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(MIN_SENSIBLE_PELLETS, playfield_get_total_pellet_count(&rules),
                                                message);
        TEST_ASSERT_LESS_THAN_UINT16_MESSAGE(MAX_SENSIBLE_PELLETS, playfield_get_total_pellet_count(&rules), message);

        /* Four, as the arcade has: one per corner of the maze, and the whole of the player's
         * ability to fight back (§10.7). */
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(EXPECTED_POWER_PELLETS, power_pellets, message);
    }
}

void test_every_maze_has_a_tunnel_that_wraps(void)
{
    /* A tunnel is marked where the corridor runs off the edge, and the mark is what makes a
     * ghost crawl there (§10.9). Both mouths of a row have to be open, or wrapping into a wall
     * would strand whatever wrapped. */
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        playfield_t rules;
        uint16_t tunnel_rows = 0U;
        char message[80];

        maze_gen_generate(&map, seed);
        playfield_load_from_map(&rules, &map);

        (void)snprintf(message, sizeof(message), "seed %u", (unsigned)seed);

        for (int16_t row = 0; row < PLAYFIELD_HEIGHT; ++row)
        {
            const cell_t left = {0, row};
            const cell_t right = {PLAYFIELD_WIDTH - 1, row};

            if (!playfield_is_tunnel(&rules, left) && !playfield_is_tunnel(&rules, right))
            {
                continue;
            }

            ++tunnel_rows;

            TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(&rules, left), message);
            TEST_ASSERT_TRUE_MESSAGE(playfield_is_walkable(&rules, right), message);
            TEST_ASSERT_TRUE_MESSAGE(playfield_is_tunnel(&rules, left), message);
            TEST_ASSERT_TRUE_MESSAGE(playfield_is_tunnel(&rules, right), message);
        }

        /* One or two, and never none: the generator rejects a grid it cannot cut a mouth
         * into, and a maze with no way round would make every chase a corridor race. */
        TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(0U, tunnel_rows, message);
        TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(2U, tunnel_rows, message);
    }
}

void test_every_maze_is_framed_by_wall(void)
{
    /* Except where a tunnel goes through it. Anywhere else, an opening in the frame would let
     * an actor wrap into a wall or out of the panel. */
    for (uint32_t seed = 1U; seed <= SEED_COUNT; ++seed)
    {
        playfield_map_t map;
        char message[80];

        maze_gen_generate(&map, seed);

        (void)snprintf(message, sizeof(message), "seed %u", (unsigned)seed);

        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            TEST_ASSERT_EQUAL_CHAR_MESSAGE(PLAYFIELD_MAP_WALL, map.rows[0][column], message);
            TEST_ASSERT_EQUAL_CHAR_MESSAGE(PLAYFIELD_MAP_WALL, map.rows[PLAYFIELD_HEIGHT - 1U][column], message);
        }

        for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
        {
            const char left = map.rows[row][0];
            const char right = map.rows[row][PLAYFIELD_WIDTH - 1U];

            TEST_ASSERT_TRUE_MESSAGE((left == PLAYFIELD_MAP_WALL) || (left == PLAYFIELD_MAP_TUNNEL), message);
            TEST_ASSERT_TRUE_MESSAGE((right == PLAYFIELD_MAP_WALL) || (right == PLAYFIELD_MAP_TUNNEL), message);
        }
    }
}

/* ==========================================================================
 * preconditions
 * ========================================================================= */

void test_null_arguments_assert(void)
{
    ASSERT_PROBE_EXPECT(maze_gen_generate(NULL, 1U), "out_map != NULL");
}
