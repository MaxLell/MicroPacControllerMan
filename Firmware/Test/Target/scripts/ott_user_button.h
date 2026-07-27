/*
 * ott_user_button.h
 *
 * user_button OTT (board bring-up): streams the live user-button state over the
 * console — a heartbeat every second plus every debounced press — so the operator
 * can see whether the pin actually toggles. Passes once the required number of
 * presses has been seen, fails on timeout with a diagnostic reason.
 */

#ifndef OTT_USER_BUTTON_H
#define OTT_USER_BUTTON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_user_button - public API
 * ========================================================================= */

bool ott_user_button_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                           uint32_t* out_parameter_size);

bool ott_user_button_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                         size_t in_reason_size);

#endif /* OTT_USER_BUTTON_H */
