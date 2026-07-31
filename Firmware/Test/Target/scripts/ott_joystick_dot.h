/*
 * ott_joystick_dot.h
 *
 * On-target test: a dot the joystick moves, drawn with partial updates only.
 *
 * The integration test for M2 — the first scenario in which input and display have
 * to be right *together*. Both halves were confirmed separately (`joystick` names
 * the keys, `display_test` shows the colours); this one fails if either the pin map
 * or the coordinate system disagrees with what an operator sees.
 */

#ifndef OTT_JOYSTICK_DOT_H
#define OTT_JOYSTICK_DOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_joystick_dot - public API
 * ========================================================================= */

/*! \brief Run the joystick dot test.
 *
 * Draws a black playfield with a yellow dot at its centre and moves the dot one
 * cell per key press, sending only the cell it left and the cell it entered. Passes
 * when all five keys have moved it and the operator confirms with B1.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when the test passed
 */
bool ott_joystick_dot_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_JOYSTICK_DOT_H */
