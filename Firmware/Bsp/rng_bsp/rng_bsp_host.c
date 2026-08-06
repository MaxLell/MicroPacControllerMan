/*
 * Host implementation of the random source, replacing rng_bsp.c in a host build. See rng_bsp.h.
 *
 * A seeded generator rather than the platform's, because this is the build the AI is trained in and
 * FR-114 asks that the same seed replay the same episode. `random()` or the clock would make a
 * fitness that moved under a fixed policy, and a training run whose numbers cannot be reproduced is
 * a training run nobody can argue with.
 */
#include "rng_bsp.h"

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * rng_bsp - private
 * ========================================================================= */

/* xorshift32: three shifts and three exclusive-ors, no multiply, no table, and a full period over
 * the 2^32 - 1 non-zero states. Nothing here needs a generator that would pass a statistical
 * battery — it decides whether a ghost leaves the house two dots early — and a small one written
 * out in full is worth more than a library call whose sequence could change under us. */
#define RNG_HOST_DEFAULT_STATE (0x1234567AUL)

static uint32_t g_state = RNG_HOST_DEFAULT_STATE;

/* ==========================================================================
 * rng_bsp - public
 * ========================================================================= */

bool rng_bsp_init(void)
{
    g_state = RNG_HOST_DEFAULT_STATE;

    return true;
}

bool rng_bsp_is_available(void)
{
    return true;
}

void rng_bsp_seed(uint32_t in_seed)
{
    /* Zero is the one state xorshift cannot leave, so it is mapped away. A caller passing 0 means
     * "the default sequence", not "no sequence". */
    g_state = (in_seed == 0U) ? RNG_HOST_DEFAULT_STATE : in_seed;
}

uint32_t rng_bsp_get_u32(void)
{
    g_state ^= g_state << 13;
    g_state ^= g_state >> 17;
    g_state ^= g_state << 5;

    return g_state;
}

uint32_t rng_bsp_get_below(uint32_t in_span)
{
    if (in_span == 0U)
    {
        return 0U;
    }

    return rng_bsp_get_u32() % in_span;
}
