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

const char g_ai_equivalence_digest[] = "b4f18357dc34867a";

const ai_equivalence_case_t g_ai_equivalence_cases[] = {
    /* --- ordinary play: seed 1, level 1 ------------------------------------- */
    {
        .what = "ordinary play",
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
                .pacman = {.column = 12U, .row = 23U, .direction = 4U, .progress = 12U},
                .ghosts =
                    {
                        {.column = 14U, .row = 11U, .direction = 0U, .progress = 255U},
                        {.column = 13U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 11U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0xE0U, 0xFFU, 0xFFU, 0x7FU, 0x42U, 0x90U, 0x20U, 0x24U,
                               0x04U, 0x09U, 0x42U, 0x4EU, 0x90U, 0x20U, 0x87U, 0xFCU, 0xF9U, 0x13U, 0x48U,
                               0x02U, 0x24U, 0x81U, 0x27U, 0x40U, 0x1EU, 0x08U, 0xFEU, 0x07U, 0x81U, 0x00U,
                               0x00U, 0x10U, 0x7EU, 0x00U, 0xE0U, 0x27U, 0x04U, 0x00U, 0x42U, 0x42U, 0x00U,
                               0x20U, 0xE4U, 0x07U, 0x00U, 0x7EU, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U,
                               0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0xC0U, 0x9FU, 0x3FU, 0x00U, 0x04U, 0x09U, 0x02U,
                               0x40U, 0x90U, 0x20U, 0x80U, 0x3FU, 0xC8U, 0x1FU, 0x48U, 0x92U, 0x24U, 0x81U,
                               0x24U, 0x49U, 0x12U, 0x4EU, 0x9EU, 0x27U, 0x27U, 0x04U, 0x09U, 0x42U, 0x42U,
                               0x90U, 0x20U, 0xE4U, 0xFFU, 0xF9U, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 10UL,
                .lives = 3U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 2U,
    },
    /* --- frightened mode: seed 1, level 1 ------------------------------------- */
    {
        .what = "frightened mode",
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
                .pacman = {.column = 26U, .row = 27U, .direction = 2U, .progress = 12U},
                .ghosts =
                    {
                        {.column = 17U, .row = 5U, .direction = 3U, .progress = 55U},
                        {.column = 7U, .row = 17U, .direction = 3U, .progress = 55U},
                        {.column = 17U, .row = 17U, .direction = 4U, .progress = 55U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0xE0U, 0xFFU, 0xFFU, 0x7FU, 0x42U, 0x90U, 0x20U, 0x24U,
                               0x04U, 0x09U, 0x42U, 0x4EU, 0x90U, 0x20U, 0x87U, 0xFCU, 0xF9U, 0x13U, 0x48U,
                               0x02U, 0x24U, 0x81U, 0x27U, 0x40U, 0x1EU, 0x08U, 0xFEU, 0x07U, 0x81U, 0x00U,
                               0x00U, 0x10U, 0x7EU, 0x00U, 0xE0U, 0x27U, 0x04U, 0x00U, 0x42U, 0x42U, 0x00U,
                               0x20U, 0xE4U, 0x07U, 0x00U, 0x7EU, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U,
                               0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x80U, 0x03U, 0xC0U, 0x01U, 0x48U, 0x80U, 0x24U, 0x80U,
                               0x04U, 0x48U, 0x02U, 0x4EU, 0x80U, 0x27U, 0x20U, 0x04U, 0x09U, 0x02U, 0x42U,
                               0x90U, 0x20U, 0xE4U, 0xFFU, 0xF9U, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 500UL,
                .lives = 3U,
                .level = 1U,
                .frightened_ghosts = 0x0FU,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 4U,
    },
    /* --- in a tunnel: seed 2, level 1 ------------------------------------- */
    {
        .what = "in a tunnel",
        .map = {.rows =
                    {
                        "############################", "#..........................#", "#.#######.########.#######.#",
                        "#o#######.########.#######o#", "#...##.......##.......##...#", "###.##.## ##.##.## ##.##.###",
                        "###.##.## ##.##.## ##.##.###", "###.##.## ##.##.## ##.##.###", "#......## ##....## ##......#",
                        "#.####### ######## #######.#", "#.####### ######## #######.#", "#...          0         ...#",
                        "###.##### ###DD### #####.###", "###.##### #HHHHHH# #####.###", "###.##.   #2H1H3H#   .##.###",
                        "###.##.## #HHHHHH# ##.##.###", "###.##.## ######## ##.##.###", "###....##          ##....###",
                        "###.##.##### ## #####.##.###", "###.##.##### ## #####.##.###", "TTT.##.......##.......##.TTT",
                        "###.#####.########.#####.###", "###.#####.########.#####.###", "#............P ............#",
                        "#.####.##.########.##.####.#", "#.####.##.########.##.####.#", "#.##...##....##....##...##.#",
                        "#.##.####.##.##.##.####.##.#", "#o##.####.##.##.##.####.##o#", "#.........##....##.........#",
                        "############################",
                    }},
        .state =
            {
                .pacman = {.column = 2U, .row = 20U, .direction = 4U, .progress = 7U},
                .ghosts =
                    {
                        {.column = 25U, .row = 1U, .direction = 3U, .progress = 8U},
                        {.column = 5U, .row = 1U, .direction = 4U, .progress = 8U},
                        {.column = 11U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0xE0U, 0xFFU, 0xFFU, 0x7FU, 0x02U, 0x02U, 0x04U, 0x24U,
                               0x20U, 0x40U, 0x40U, 0xCEU, 0x9FU, 0x3FU, 0x87U, 0x04U, 0x09U, 0x12U, 0x48U,
                               0x90U, 0x20U, 0x81U, 0x04U, 0x09U, 0x12U, 0x7EU, 0xF0U, 0xE0U, 0x27U, 0x00U,
                               0x00U, 0x40U, 0x02U, 0x00U, 0x00U, 0xE4U, 0x00U, 0x00U, 0x70U, 0x08U, 0x00U,
                               0x00U, 0x81U, 0x00U, 0x00U, 0x10U, 0x48U, 0x00U, 0x20U, 0x81U, 0x04U, 0x00U,
                               0x12U, 0x48U, 0x00U, 0x20U, 0x01U, 0x00U, 0x00U, 0x1EU, 0x00U, 0x00U, 0x20U,
                               0x01U, 0x00U, 0x00U, 0x12U, 0x00U, 0x9CU, 0x3FU, 0x81U, 0x00U, 0x40U, 0x10U,
                               0x08U, 0x00U, 0x04U, 0xE1U, 0x1FU, 0xF8U, 0x7FU, 0x42U, 0x02U, 0x24U, 0x24U,
                               0x24U, 0x40U, 0x42U, 0x72U, 0x9EU, 0xE7U, 0x24U, 0x21U, 0x49U, 0x48U, 0x12U,
                               0x92U, 0x84U, 0xE4U, 0x3FU, 0xCFU, 0x7FU, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x04U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 190UL,
                .lives = 3U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 3U,
    },
    /* --- a life just lost: seed 1, level 1 ------------------------------------- */
    {
        .what = "a life just lost",
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
                .pacman = {.column = 13U, .row = 23U, .direction = 0U, .progress = 255U},
                .ghosts =
                    {
                        {.column = 14U, .row = 11U, .direction = 0U, .progress = 255U},
                        {.column = 13U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 11U, .row = 14U, .direction = 0U, .progress = 255U},
                        {.column = 15U, .row = 14U, .direction = 0U, .progress = 255U},
                    },
                .has_pellet = {0x00U, 0x00U, 0x00U, 0xE0U, 0xFFU, 0xFFU, 0x7FU, 0x42U, 0x90U, 0x20U, 0x24U,
                               0x04U, 0x09U, 0x42U, 0x4EU, 0x90U, 0x20U, 0x87U, 0xFCU, 0xF9U, 0x13U, 0x48U,
                               0x02U, 0x24U, 0x81U, 0x27U, 0x40U, 0x1EU, 0x08U, 0xFEU, 0x07U, 0x81U, 0x00U,
                               0x00U, 0x10U, 0x7EU, 0x00U, 0xE0U, 0x27U, 0x04U, 0x00U, 0x42U, 0x42U, 0x00U,
                               0x20U, 0xE4U, 0x07U, 0x00U, 0x7EU, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U,
                               0x02U, 0x40U, 0x00U, 0x20U, 0x00U, 0x04U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                               0x00U, 0x00U, 0x00U, 0x80U, 0x03U, 0x00U, 0x00U, 0x48U, 0x00U, 0x00U, 0x80U,
                               0x04U, 0x00U, 0x00U, 0x4EU, 0x00U, 0x00U, 0x20U, 0x04U, 0x00U, 0x00U, 0x42U,
                               0x00U, 0x00U, 0xE0U, 0x3FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .is_power = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U,
                             0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x20U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
                             0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
                .score = 860UL,
                .lives = 2U,
                .level = 1U,
                .frightened_ghosts = 0x00U,
                .are_frightened_ghosts_flashing = false,
            },
        .expected_direction = 4U,
    },
};

const size_t g_ai_equivalence_case_count = 4;
