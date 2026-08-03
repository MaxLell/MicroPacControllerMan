#include "ott_high_score.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "high_score.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_high_score - private
 * ========================================================================= */

/* Three values that are distinct, ordered, and unlike anything a game produces, so a table
 * left behind by a failed run is recognisable for what it is. */
#define FIRST_SCORE  (31337UL)
#define SECOND_SCORE (2222UL)
#define THIRD_SCORE  (111UL)

/* Put back what was there before the test, smallest first: `high_score_offer` inserts by
 * rank, so offering in ascending order reproduces the original order exactly. */
static void prv_restore(const uint32_t* const in_saved)
{
    (void)high_score_reset();

    for (uint8_t place = HIGH_SCORE_COUNT; place > 0U; --place)
    {
        (void)high_score_offer(in_saved[place - 1U]);
    }
}

/* ==========================================================================
 * ott_high_score - public
 * ========================================================================= */

bool ott_high_score_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    static const uint32_t k_written[HIGH_SCORE_COUNT] = {FIRST_SCORE, SECOND_SCORE, THIRD_SCORE};
    uint32_t saved[HIGH_SCORE_COUNT];
    uint32_t erase_start_tick;
    uint32_t write_start_tick;
    uint32_t erase_ms;
    uint32_t write_ms;
    bool is_correct = true;

    (void)in_parameter;

    cli_print("High score: the table in real flash — erase, program, read back.");

    high_score_init();

    for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        saved[place] = high_score_get(place);
    }

    cli_print("  stored now: %lu / %lu / %lu", (unsigned long)saved[0], (unsigned long)saved[1],
              (unsigned long)saved[2]);

    erase_start_tick = systick_bsp_get_tick();

    if (!high_score_reset())
    {
        (void)snprintf(out_reason, in_reason_size, "the page could not be erased");

        return false;
    }

    erase_ms = systick_bsp_get_tick() - erase_start_tick;

    high_score_init();

    if (high_score_get_best() != 0U)
    {
        (void)snprintf(out_reason, in_reason_size, "an erased page did not read back empty");
        prv_restore(saved);

        return false;
    }

    /* Ascending, so each one displaces the last and all three end up stored. */
    write_start_tick = systick_bsp_get_tick();

    for (uint8_t place = HIGH_SCORE_COUNT; place > 0U; --place)
    {
        (void)high_score_offer(k_written[place - 1U]);
    }

    write_ms = systick_bsp_get_tick() - write_start_tick;

    /* Re-reading is the whole point: until now everything could have come from RAM. */
    high_score_init();

    for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        if (high_score_get(place) != k_written[place])
        {
            is_correct = false;
        }
    }

    cli_print("  wrote %lu / %lu / %lu, read back %lu / %lu / %lu", (unsigned long)k_written[0],
              (unsigned long)k_written[1], (unsigned long)k_written[2], (unsigned long)high_score_get(0U),
              (unsigned long)high_score_get(1U), (unsigned long)high_score_get(2U));
    cli_print("  erase %lu ms, %u writes %lu ms — far too slow for a frame, which is why a run",
              (unsigned long)erase_ms, (unsigned)HIGH_SCORE_COUNT, (unsigned long)write_ms);
    cli_print("  is offered to the table once, when it ends.");

    prv_restore(saved);

    if (!is_correct)
    {
        (void)snprintf(out_reason, in_reason_size, "what was read back is not what was written");
    }
    else
    {
        cli_print("  table restored to what it was. `reset`, then `highscore`, shows it survived.");
    }

    return is_correct;
}
