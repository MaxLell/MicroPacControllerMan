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
 * rank, so offering in ascending order reproduces the original order exactly. All three tables,
 * because the test overwrites all three. */
static void prv_restore(const uint32_t in_saved[HIGH_SCORE_TABLE_COUNT][HIGH_SCORE_COUNT])
{
    (void)high_score_reset();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = HIGH_SCORE_COUNT; place > 0U; --place)
        {
            (void)high_score_offer(table, in_saved[table][place - 1U]);
        }
    }
}

/* ==========================================================================
 * ott_high_score - public
 * ========================================================================= */

bool ott_high_score_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    static const uint32_t k_written[HIGH_SCORE_COUNT] = {FIRST_SCORE, SECOND_SCORE, THIRD_SCORE};
    uint32_t saved[HIGH_SCORE_TABLE_COUNT][HIGH_SCORE_COUNT];
    uint32_t erase_start_tick;
    uint32_t write_start_tick;
    uint32_t erase_ms;
    uint32_t write_ms;
    bool is_correct = true;

    (void)in_parameter;

    cli_print("High score: the three tables in real flash — erase, program, read back.");

    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
        {
            saved[table][place] = high_score_get(table, place);
        }

        cli_print("  table %u stored now: %lu / %lu / %lu", (unsigned)table, (unsigned long)saved[table][0],
                  (unsigned long)saved[table][1], (unsigned long)saved[table][2]);
    }

    erase_start_tick = systick_bsp_get_tick();

    if (!high_score_reset())
    {
        (void)snprintf(out_reason, in_reason_size, "the page could not be erased");

        return false;
    }

    erase_ms = systick_bsp_get_tick() - erase_start_tick;

    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        if (high_score_get_best(table) != 0U)
        {
            (void)snprintf(out_reason, in_reason_size, "an erased page did not read back empty");
            prv_restore(saved);

            return false;
        }
    }

    /* A different triple per table, so a page that stored one table three times — or the same table
     * three times over — cannot pass. Ascending within a table, so each score displaces the last and
     * all three end up stored. */
    write_start_tick = systick_bsp_get_tick();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = HIGH_SCORE_COUNT; place > 0U; --place)
        {
            (void)high_score_offer(table, k_written[place - 1U] + table);
        }
    }

    write_ms = systick_bsp_get_tick() - write_start_tick;

    /* Re-reading is the whole point: until now everything could have come from RAM. */
    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
        {
            if (high_score_get(table, place) != (k_written[place] + table))
            {
                is_correct = false;
            }
        }

        cli_print("  table %u wrote %lu / %lu / %lu, read back %lu / %lu / %lu", (unsigned)table,
                  (unsigned long)(k_written[0] + table), (unsigned long)(k_written[1] + table),
                  (unsigned long)(k_written[2] + table), (unsigned long)high_score_get(table, 0U),
                  (unsigned long)high_score_get(table, 1U), (unsigned long)high_score_get(table, 2U));
    }
    cli_print("  erase %lu ms, %u writes %lu ms — far too slow for a frame, which is why a run",
              (unsigned long)erase_ms, (unsigned)(HIGH_SCORE_COUNT * HIGH_SCORE_TABLE_COUNT), (unsigned long)write_ms);
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
