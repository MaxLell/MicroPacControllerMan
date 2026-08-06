/*
 * ott_ai_equivalence_states.c
 *
 * GENERATED — do not edit. Written by Training/record_states.c, which is built as
 * `pacman_ai_record` in the host configuration:
 *
 *   ./build-host/pacman_ai_record > Test/Target/scripts/ott_ai_equivalence_states.c
 *
 * The states the host played through, and the direction the host build chose about each of
 * them. The target replays them and must agree (FR-039, VT-INT-024).
 *
 * Tied to one weight table: the expected directions belong to the network below, so
 * re-exporting weights means re-recording these. The digest is carried along and the test
 * refuses to run when it does not match, which is what stops a stale file being read as a
 * porting fault.
 */

#include "ott_ai_equivalence_states.h"

const char g_ai_equivalence_digest[] = "a082e6ea61e8f6fa";

const ai_equivalence_case_t g_ai_equivalence_cases[] = {
    /* --- ordinary play: the normal maze, level 1 ------------------------------------- */
    {
        .what = "ordinary play",
        .map = {.rows =
                    {
                        "############################", "#............##............#", "#.####.#####.##.#####.####.#",
                        "#o####.#####.##.#####.####o#", "#.####.#####.##.#####.####.#", "#..........................#",
                        "#.####.##.########.##.####.#", "#.####.##.########.##.####.#", "#......##....##....##......#",
                        "######.##### ## #####.######", "######.##### ## #####.######", "######.##     0    ##.######",
                        "######.## ###DD### ##.######", "######.## #HHHHHH# ##.######", "TTTTTT.   #2H1H3H#   .TTTTTT",
                        "######.## #HHHHHH# ##.######", "######.## ######## ##.######", "######.##          ##.######",
                        "######.## ######## ##.######", "######.## ######## ##.######", "#............##............#",
                        "#.####.#####.##.#####.####.#", "#.####.#####.##.#####.####.#", "#o..##.......P .......##..o#",
                        "###.##.##.########.##.##.###", "###.##.##.########.##.##.###", "#......##....##....##......#",
                        "#.##########.##.##########.#", "#.##########.##.##########.#", "#..........................#",
                        "############################",
                    }},
        .state =
            {
                .pacman = {.column = 12U, .row = 23U, .direction = 4U, .progress = 12U},
                .ghosts =
                    {
                        {.column = 14U, .row = 11U, .direction = 0U, .progress = 255U},
                        {.column = 13U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 11U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0xE0U, 0xFFU, 0xF9U, 0x7FU, 0x42U, 0x90U, 0x20U, 0x24U,
                               0x04U, 0x09U, 0x42U, 0x42U, 0x90U, 0x20U, 0xE4U, 0xFFU, 0xFFU, 0x7FU, 0x42U,
                               0x02U, 0x24U, 0x24U, 0x24U, 0x40U, 0x42U, 0x7EU, 0x9EU, 0xE7U, 0x07U, 0x04U,
                               0x00U, 0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x40U, 0x00U,
                               0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U,
                               0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x40U, 0x00U, 0x20U,
                               0x00U, 0x04U, 0x00U, 0x02U, 0xFEU, 0x9FU, 0xFFU, 0x27U, 0x04U, 0x09U, 0x42U,
                               0x42U, 0x90U, 0x20U, 0xE4U, 0xFCU, 0xF8U, 0x73U, 0x48U, 0x02U, 0x24U, 0x81U,
                               0x24U, 0x40U, 0x12U, 0x7EU, 0x9EU, 0xE7U, 0x27U, 0x00U, 0x09U, 0x40U, 0x02U,
                               0x90U, 0x00U, 0xE4U, 0xFFU, 0xFFU, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 10UL,
                .lives = 3U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 1U,
    },
    /* --- frightened mode: the normal maze, level 1 ------------------------------------- */
    {
        .what = "frightened mode",
        .map = {.rows =
                    {
                        "############################", "#............##............#", "#.####.#####.##.#####.####.#",
                        "#o####.#####.##.#####.####o#", "#.####.#####.##.#####.####.#", "#..........................#",
                        "#.####.##.########.##.####.#", "#.####.##.########.##.####.#", "#......##....##....##......#",
                        "######.##### ## #####.######", "######.##### ## #####.######", "######.##     0    ##.######",
                        "######.## ###DD### ##.######", "######.## #HHHHHH# ##.######", "TTTTTT.   #2H1H3H#   .TTTTTT",
                        "######.## #HHHHHH# ##.######", "######.## ######## ##.######", "######.##          ##.######",
                        "######.## ######## ##.######", "######.## ######## ##.######", "#............##............#",
                        "#.####.#####.##.#####.####.#", "#.####.#####.##.#####.####.#", "#o..##.......P .......##..o#",
                        "###.##.##.########.##.##.###", "###.##.##.########.##.##.###", "#......##....##....##......#",
                        "#.##########.##.##########.#", "#.##########.##.##########.#", "#..........................#",
                        "############################",
                    }},
        .state =
            {
                .pacman = {.column = 1U, .row = 3U, .direction = 2U, .progress = 7U},
                .ghosts =
                    {
                        {.column = 23U, .row = 1U, .direction = 3U, .progress = 159U},
                        {.column = 6U, .row = 8U, .direction = 3U, .progress = 159U},
                        {.column = 14U, .row = 11U, .direction = 3U, .progress = 159U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0x00U, 0xF8U, 0xF9U, 0x7FU, 0x00U, 0x90U, 0x20U, 0x04U,
                               0x00U, 0x09U, 0x42U, 0x02U, 0x90U, 0x20U, 0xE4U, 0xFBU, 0xFFU, 0x7FU, 0x02U,
                               0x02U, 0x24U, 0x24U, 0x20U, 0x40U, 0x42U, 0x3EU, 0x9EU, 0xE7U, 0x07U, 0x00U,
                               0x00U, 0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U,
                               0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U,
                               0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x20U,
                               0x00U, 0x00U, 0x00U, 0x02U, 0x3EU, 0x80U, 0xFFU, 0x27U, 0x04U, 0x08U, 0x42U,
                               0x42U, 0x80U, 0x20U, 0xE4U, 0xFCU, 0xF8U, 0x73U, 0x48U, 0x02U, 0x24U, 0x81U,
                               0x24U, 0x40U, 0x12U, 0x7EU, 0x9EU, 0xE7U, 0x27U, 0x00U, 0x09U, 0x40U, 0x02U,
                               0x90U, 0x00U, 0xE4U, 0xFFU, 0xFFU, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 400UL,
                .lives = 3U,
                .level = 1U,
                .frightened_ghosts = 0x0FU,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 2U,
    },
    /* --- in a tunnel: generated maze, seed 1, level 1 ------------------------------------- */
    {
        .what = "in a tunnel",
        .map = {.rows =
                    {
                        "############################", "#..........................#", "#.####.#####.##.#####.####.#",
                        "#o####.#####.##.#####.####o#", "#...##.#####.##.#####.##...#", "###.##.......##.......##.###",
                        "###.##.##.########.##.##.###", "###....##.########.##....###", "###.#####..........#####.###",
                        "###.##### ######## #####.###", "#......## ######## ##......#", "#.####.##     0    ##.####.#",
                        "#.####.## ###DD### ##.####.#", "#......## #HHHHHH# ##......#", "### ##.## #2H1H3H# ##.## ###",
                        "### ##.## #HHHHHH# ##.## ###", "### ##.## ######## ##.## ###", "TTTT##.              .##TTTT",
                        "######### ######## #########", "######### ######## #########", "TTTT##.......##.......##TTTT",
                        "### ##.#####.##.#####.## ###", "### ##.#####.##.#####.## ###", "###.......##.P .##.......###",
                        "###.##.##.##.##.##.##.##.###", "###.##.##.##.##.##.##.##.###", "#...##.##....##....##.##...#",
                        "#o####.#####.##.#####.####o#", "#.####.#####.##.#####.####.#", "#............##............#",
                        "############################",
                    }},
        .state =
            {
                .pacman = {.column = 3U, .row = 17U, .direction = 2U, .progress = 6U},
                .ghosts =
                    {
                        {.column = 3U, .row = 14U, .direction = 2U, .progress = 195U},
                        {.column = 22U, .row = 13U, .direction = 4U, .progress = 221U},
                        {.column = 23U, .row = 13U, .direction = 4U, .progress = 221U},
                        {.column = 6U, .row = 14U, .direction = 1U, .progress = 212U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x06U, 0x00U, 0x00U, 0x40U, 0x00U, 0x00U,
                               0x00U, 0x04U, 0x00U, 0x00U, 0x7CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 3070UL,
                .lives = 1U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 4U,
    },
    /* --- a life just lost: the normal maze, level 1 ------------------------------------- */
    {
        .what = "a life just lost",
        .map = {.rows =
                    {
                        "############################", "#............##............#", "#.####.#####.##.#####.####.#",
                        "#o####.#####.##.#####.####o#", "#.####.#####.##.#####.####.#", "#..........................#",
                        "#.####.##.########.##.####.#", "#.####.##.########.##.####.#", "#......##....##....##......#",
                        "######.##### ## #####.######", "######.##### ## #####.######", "######.##     0    ##.######",
                        "######.## ###DD### ##.######", "######.## #HHHHHH# ##.######", "TTTTTT.   #2H1H3H#   .TTTTTT",
                        "######.## #HHHHHH# ##.######", "######.## ######## ##.######", "######.##          ##.######",
                        "######.## ######## ##.######", "######.## ######## ##.######", "#............##............#",
                        "#.####.#####.##.#####.####.#", "#.####.#####.##.#####.####.#", "#o..##.......P .......##..o#",
                        "###.##.##.########.##.##.###", "###.##.##.########.##.##.###", "#......##....##....##......#",
                        "#.##########.##.##########.#", "#.##########.##.##########.#", "#..........................#",
                        "############################",
                    }},
        .state =
            {
                .pacman = {.column = 13U, .row = 23U, .direction = 0U, .progress = 255U},
                .ghosts =
                    {
                        {.column = 14U, .row = 11U, .direction = 0U, .progress = 255U},
                        {.column = 13U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 11U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xE0U, 0x7FU, 0x00U, 0x00U, 0x20U, 0x04U,
                               0x00U, 0x00U, 0x42U, 0x00U, 0x00U, 0x20U, 0x04U, 0x00U, 0x86U, 0x7FU, 0x00U,
                               0x00U, 0x20U, 0x04U, 0x00U, 0x00U, 0x42U, 0x00U, 0x00U, 0xE0U, 0x07U, 0x00U,
                               0x00U, 0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U,
                               0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U,
                               0x02U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x20U,
                               0x00U, 0x00U, 0x00U, 0x02U, 0x3EU, 0x80U, 0xFFU, 0x27U, 0x04U, 0x08U, 0x42U,
                               0x42U, 0x80U, 0x20U, 0xE4U, 0xFCU, 0xF8U, 0x73U, 0x48U, 0x02U, 0x24U, 0x81U,
                               0x24U, 0x40U, 0x12U, 0x7EU, 0x9EU, 0xE7U, 0x27U, 0x00U, 0x09U, 0x40U, 0x02U,
                               0x90U, 0x00U, 0xE4U, 0xFFU, 0xFFU, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 890UL,
                .lives = 2U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 4U,
    },
};

const size_t g_ai_equivalence_case_count = 4;
