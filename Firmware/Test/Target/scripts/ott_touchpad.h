/*
 * ott_touchpad.h
 *
 * touchpad OTT (VT-INT-007): prints the live touch state (position and
 * touch-present) over the console so the operator can confirm the reading follows
 * their finger. Ends on a user-button press, with a safety cap.
 */

#ifndef OTT_TOUCHPAD_H
#define OTT_TOUCHPAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_touchpad - public API
 * ========================================================================= */

bool ott_touchpad_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                        uint32_t* out_parameter_size);

bool ott_touchpad_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                      size_t in_reason_size);

#endif /* OTT_TOUCHPAD_H */
