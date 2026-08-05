/*
 * ott_pacman_ai.h
 *
 * On-target test: the AI takes over on the board, and gives Pacman back (VT-INT-023).
 *
 * `pacman` is the same game played by hand; this is the same game with a button that hands it to the
 * agent. What the operator confirms is the four things a script cannot judge — that the HUD says the
 * AI has taken over (FR-032), that the stick is dead while it plays (FR-031), that it is still
 * playing after a level turns over and after a life is lost (FR-033), and that control comes back on
 * the next press.
 *
 * It also reports what a frame costs with inference in it, against NFR-006.
 *
 * Manual, button-confirmed — the same shape as `pacman`.
 */

#ifndef OTT_PACMAN_AI_H
#define OTT_PACMAN_AI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_pacman_ai - public API
 * ========================================================================= */

/*! \brief Start a run and let the board button hand it to the AI and back.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when confirmed at the board
 */
bool ott_pacman_ai_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_PACMAN_AI_H */
