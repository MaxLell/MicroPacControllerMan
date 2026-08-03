/*
 * ott_pacman.h
 *
 * On-target test: the game itself, played at the board.
 *
 * The end-to-end scenario for M3, and the one test no host build can stand in for. The
 * host application runs the same `game_session` frame in an SDL window, which is evidence
 * about the rules and the drawing but says nothing about this panel, this joystick or
 * this frame budget. Here they are all real.
 *
 * Manual by nature: what it checks — that the maze looks right, that the stick turns
 * Pacman when it should, that the ghosts behave — is a judgement, so the verdict comes
 * from the operator's B1 press rather than from an assertion.
 */

#ifndef OTT_PACMAN_H
#define OTT_PACMAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_pacman - public API
 * ========================================================================= */

/*! \brief Play the game, then say whether it was right.
 *
 * Starts a run immediately — no menu, no start screen — and hands the joystick to the
 * operator. Reports level, lives and score on the console as they change, so the run is
 * followable without looking at the panel. Passes when B1 is pressed, fails on the
 * timeout.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when the operator confirmed the game plays correctly
 */
bool ott_pacman_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_PACMAN_H */
