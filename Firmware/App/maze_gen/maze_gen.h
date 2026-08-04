/*
 * maze_gen.h
 *
 * Builds a random maze that looks like one of Pacman's, in the map legend `playfield`
 * loads (FR-026). One seed gives one maze, always the same one, so a level is
 * reproducible and a unit test can pin a specific layout down.
 *
 * It is a port of the **tetris-stacking generator** from
 * [shaunlebron/pacman-mazegen](https://github.com/shaunlebron/pacman-mazegen), the
 * `tetris/mapgen.js` of that repository, whose approach is: build a small 9 x 5 grid by
 * stacking tetris-like pieces against the left edge, upscale it by three, then mirror it.
 * Mirroring is why only half the maze is ever generated, and it is also why the result
 * reads as a Pacman maze rather than as a maze — the arcade's are symmetric too.
 *
 * The port is deliberately **faithful rather than tidied**: the same decisions in the same
 * order, consuming the same random numbers, so its output can be compared against the
 * original's byte for byte (see `Test/Host/test_maze_gen.c` and
 * [M4 Random Mazes](../../../Docu/Design/M4-Random-Mazes.md)). Where the JavaScript relies
 * on a language accident, the accident is reproduced and commented rather than corrected —
 * correcting it would silently make this a different generator.
 *
 * Pure logic: no hardware, no messages, no allocation. The whole working set is one
 * `playfield_map_t` plus a 45-cell scratch grid.
 */

#ifndef MAZE_GEN_H
#define MAZE_GEN_H

#include <stdint.h>

#include "playfield.h"

/* ==========================================================================
 * maze_gen - public API
 * ========================================================================= */

/*! \brief Generate the maze belonging to a seed.
 *
 * Always succeeds. The generator rejects its own bad attempts and retries internally —
 * unreachable pellets, walls cutting a straight line through the tunnels, corners that
 * cannot be drawn — so a caller never sees a failure and never has to check one. Roughly
 * three attempts are needed per maze; the worst seen in 2000 seeds was 24.
 *
 * The ghost house, its gate, the four ghost starting cells and Pacman's start are stamped
 * in at the arcade's own coordinates. That is not a shortcut: the generator's own grid puts
 * the house in exactly that place, so the release order, the spawn positions and the
 * scatter targets keep the meaning they were verified with (§10.4, §10.5) while the maze
 * around them changes.
 *
 * \param[out]      out_map: filled in completely, must not be `NULL`
 * \param[in]       in_seed: any value; `0` is allowed and is not a special case
 */
void maze_gen_generate(playfield_map_t* out_map, uint32_t in_seed);

#endif /* MAZE_GEN_H */
