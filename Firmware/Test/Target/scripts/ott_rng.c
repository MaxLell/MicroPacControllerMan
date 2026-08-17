#include "ott_rng.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "rng_bsp.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_rng - private
 * ========================================================================= */

/* Enough words that "they are all the same" is a statement worth making, and few enough that the
 * whole test is over in microseconds. */
#define OTT_RNG_WORD_COUNT (8U)

/* ==========================================================================
 * ott_rng - public
 * ========================================================================= */

bool ott_rng_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    uint32_t words[OTT_RNG_WORD_COUNT];
    uint32_t start_tick;
    uint32_t elapsed_ms;
    bool is_all_zero = true;
    bool is_constant = true;

    (void)in_parameter;

    cli_print("RNG: the MCU's own random source, on silicon (FR-045).");

    if (!rng_bsp_is_available())
    {
        /* Already initialised by `app_main`; asking rather than initialising again, because the
         * question this test answers is whether the *firmware's* generator came up, not whether one
         * can be made to. */
        (void)snprintf(out_reason, in_reason_size, "the generator did not come up: check HSI48 and the RNG clock");

        return false;
    }

    start_tick = systick_bsp_get_tick();

    for (uint8_t index = 0U; index < OTT_RNG_WORD_COUNT; ++index)
    {
        words[index] = rng_bsp_get_u32();

        if (words[index] != 0U)
        {
            is_all_zero = false;
        }

        if (words[index] != words[0])
        {
            is_constant = false;
        }
    }

    elapsed_ms = systick_bsp_get_tick() - start_tick;

    for (uint8_t index = 0U; index < OTT_RNG_WORD_COUNT; ++index)
    {
        cli_print("  0x%08lX", (unsigned long)words[index]);
    }

    cli_print("  %u words in %lu ms", (unsigned)OTT_RNG_WORD_COUNT, (unsigned long)elapsed_ms);

    if (is_all_zero)
    {
        /* Zero is what an unavailable generator returns by design, so all-zero means the "available"
         * flag and the peripheral disagree — a worse fault than not coming up at all. */
        (void)snprintf(out_reason, in_reason_size, "every word was zero");

        return false;
    }

    if (is_constant)
    {
        (void)snprintf(out_reason, in_reason_size, "every word was the same: 0x%08lX", (unsigned long)words[0]);

        return false;
    }

    cli_print("  the words differ, so the game's timings will vary (FR-044).");

    return true;
}
