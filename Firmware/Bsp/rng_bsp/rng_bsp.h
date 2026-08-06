/*
 * rng_bsp.h
 *
 * Where every random number in this firmware comes from (FR-045).
 *
 * One shared header, one `.c` per platform, selected in `CMakeLists.txt` — the platform-port
 * pattern `systick_bsp` established. The two implementations differ in what "random" means, and
 * the difference is deliberate rather than a limitation:
 *
 * - **Target:** the STM32U545's own RNG peripheral. A hardware entropy source, so nothing is
 *   reproducible and nothing has to be seeded. That is what the owner asked for, and it is also
 *   the honest source for a game: a player who could predict the ghosts would be playing a
 *   different game.
 * - **Host:** a seeded generator, because the host is where the AI is *trained* and where
 *   FR-114 asks that the same seed replay the same episode. An episode that could not be
 *   replayed could not be debugged, and a fitness that changed under a fixed policy would be
 *   measuring the generator rather than the policy.
 *
 * So #rng_bsp_seed is meaningful on the host and ignored on the target. That asymmetry is stated
 * in the API rather than hidden behind an `#ifdef` at the call site: callers ask for a number, and
 * only the two people who need to care — the trainer and the board — know where it came from.
 *
 * **`maze_gen` is not a caller.** It keeps its own seeded PRNG, because FR-029 requires the same
 * seed to give the same maze and the port is checked against its original byte for byte. What it
 * takes from here is the *seed* of a run, once, which on the target is now hardware entropy
 * instead of the tick a player pressed start on.
 */

#ifndef RNG_BSP_H
#define RNG_BSP_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * rng_bsp - public API
 * ========================================================================= */

/*! \brief Bring the generator up. Must be called once before anything else asks for a number.
 *
 * On the target this enables the RNG peripheral's clock and initialises it through the HAL. It is
 * done **here rather than in the CubeMX export**: the RNG has no pins, so all the `.ioc` would add
 * is one clock enable, and a hand edit to generated code is the thing a regeneration silently
 * drops ([DEC-012](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)). In this file it
 * survives.
 *
 * \return          `true` when the generator is usable
 */
bool rng_bsp_init(void);

/*! \brief Whether the generator came up.
 *
 * Worth asking rather than assuming: a peripheral that failed to initialise would otherwise hand
 * out one repeated value, and a game whose ghosts all left the house at the same moment would look
 * like a rule rather than a fault.
 */
bool rng_bsp_is_available(void);

/*! \brief Set the sequence, where that is a thing that can be set.
 *
 * The host's generator starts from `in_seed`; the target ignores it, because hardware entropy has
 * no sequence to choose. A caller that wants a reproducible episode calls this and gets one on the
 * host; on the board it is a no-op, and that is the intended difference.
 *
 * \param[in]       in_seed: any value; `0` is allowed and does not degenerate
 */
void rng_bsp_seed(uint32_t in_seed);

/*! \brief A 32-bit random number.
 *
 * Returns `0` when the generator is unavailable, which a caller may treat as "no jitter" — the
 * game then plays its nominal timings rather than refusing to play.
 */
uint32_t rng_bsp_get_u32(void);

/*! \brief A random number in `[0, in_span)`, or `0` when `in_span` is `0`.
 *
 * The modulo bias is left in on purpose. The spans here are a handful of dots and a couple of
 * seconds against a 2^32 range, so the bias is one part in ten million — far below anything a
 * player or a fitness function could notice, and rejection sampling would add a loop whose worst
 * case is unbounded to a game loop that has a frame to catch.
 *
 * \param[in]       in_span: exclusive upper bound
 */
uint32_t rng_bsp_get_below(uint32_t in_span);

#endif /* RNG_BSP_H */
