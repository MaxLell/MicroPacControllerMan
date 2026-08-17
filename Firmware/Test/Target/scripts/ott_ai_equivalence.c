#include "ott_ai_equivalence.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Cli.h"
#include "ai_weights.h"
#include "msg.h"
#include "ott_ai_equivalence_states.h"
#include "pacman_ai.h"
#include "playfield.h"

/* ==========================================================================
 * ott_ai_equivalence - private
 * ========================================================================= */

/* The playfield each case is replayed against. File scope rather than on the stack: it is 7 kB and
 * the OTT runs from the same loop the game does (NFR-008). */
static playfield_t g_playfield;

static const char* const g_direction_names[] = {"none", "north", "south", "west", "east"};

static const char* prv_get_direction_name(uint8_t in_direction)
{
    if (in_direction >= (sizeof(g_direction_names) / sizeof(g_direction_names[0])))
    {
        return "?";
    }

    return g_direction_names[in_direction];
}

/* Put the maze and the pellets back the way they were when the state was recorded.
 *
 * `playfield_load_from_map` gives a full maze, and the recorded state is somewhere in the middle of
 * a level — so every pellet the state says is gone is eaten again here. It matters: one of the
 * twenty-three features is how much of the level is left, and it comes from the playfield rather
 * than from the state. Loading the maze and stopping there would put that feature at 1.0 and could
 * change the answer for a reason that has nothing to do with the port.
 */
static void prv_restore(const ai_equivalence_case_t* const in_case)
{
    playfield_load_from_map(&g_playfield, &in_case->map);

    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            if (!msg_cell_bitmap_get(in_case->state.has_pellet, column, row))
            {
                const cell_t cell = {.x = (int16_t)column, .y = (int16_t)row};

                (void)playfield_eat_pellet(&g_playfield, cell);
            }
        }
    }
}

/* ==========================================================================
 * ott_ai_equivalence - public
 * ========================================================================= */

bool ott_ai_equivalence_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    (void)in_parameter;

    cli_print("AI equivalence: the ported inference must choose what the host chose (FR-039).");
    cli_print("weights %s, %u recorded states", AI_WEIGHTS_DIGEST, (unsigned)g_ai_equivalence_case_count);

    if (!pacman_ai_is_available())
    {
        (void)snprintf(out_reason, in_reason_size, "the weight table cannot be evaluated on this build");

        return false;
    }

    /* Before anything else: the expectations belong to one weight table. A mismatch here is a
     * stale recording, not a porting fault, and saying so is the difference between a five-minute
     * fix and an afternoon. */
    if (strcmp(g_ai_equivalence_digest, AI_WEIGHTS_DIGEST) != 0)
    {
        (void)snprintf(out_reason, in_reason_size, "recorded against weights %s, this build carries %s — re-record",
                       g_ai_equivalence_digest, AI_WEIGHTS_DIGEST);

        return false;
    }

    for (size_t index = 0U; index < g_ai_equivalence_case_count; ++index)
    {
        const ai_equivalence_case_t* const entry = &g_ai_equivalence_cases[index];

        prv_restore(entry);

        const direction_e chosen = pacman_ai_decide(&entry->state, &g_playfield);

        cli_print("  %-18s pacman (%2u,%2u) facing %-5s -> %-5s, host said %s", entry->what,
                  (unsigned)entry->state.pacman.column, (unsigned)entry->state.pacman.row,
                  prv_get_direction_name(entry->state.pacman.direction), prv_get_direction_name((uint8_t)chosen),
                  prv_get_direction_name(entry->expected_direction));

        if ((uint8_t)chosen != entry->expected_direction)
        {
            (void)snprintf(out_reason, in_reason_size, "%s: chose %s, the host chose %s", entry->what,
                           prv_get_direction_name((uint8_t)chosen), prv_get_direction_name(entry->expected_direction));

            return false;
        }
    }

    cli_print("  every recorded state decided the same way here");

    return true;
}
