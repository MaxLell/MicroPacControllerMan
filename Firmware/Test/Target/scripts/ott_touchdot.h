/*
 * ott_touchdot.h
 *
 * touchdot OTT: combined display and touchpad test. A dot on the panel tracks the
 * finger position read from the pad, so the operator can confirm both devices work
 * together and the coordinate mapping is right. Ends on a user-button press.
 */

#ifndef OTT_TOUCHDOT_H
#define OTT_TOUCHDOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_touchdot - public API
 * ========================================================================= */

bool ott_touchdot_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                        uint32_t* out_parameter_size);

bool ott_touchdot_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                      size_t in_reason_size);

#endif /* OTT_TOUCHDOT_H */
