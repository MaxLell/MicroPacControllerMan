/*
 * ott_ai_high_score.h
 *
 * On-target test: a run the AI touched never reaches NVM (VT-INT-025, FR-034).
 *
 * Two runs in one scenario, deliberately. A lockout that simply broke high scores altogether would
 * pass the first half — so the second half is a player-only run of a *lower* score that must get
 * in. Both halves have to be here, or neither says anything.
 *
 * What the target adds over the unit test is the flash: the table is the page the linker reserved,
 * written through `flash_bsp`, and read back through an ICACHE that has already once answered with
 * what the page used to hold ([DEC-025](../../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).
 *
 * Automatic, and it leaves the table as it found it. It plays two runs to game over, so it takes a
 * few minutes — the only slow test in the automatic suite, and it says so on the console.
 */

#ifndef OTT_AI_HIGH_SCORE_H
#define OTT_AI_HIGH_SCORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_ai_high_score - public API
 * ========================================================================= */

/*! \brief Play an AI run and a player run, and check which of them reached the table.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when the AI's run stayed out and the player's got in
 */
bool ott_ai_high_score_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_AI_HIGH_SCORE_H */
