/*
 * ott_joystick.h
 *
 * joystick OTT (board bring-up): names each key of the shield's joystick as it is
 * pressed, and passes once all five have been seen.
 *
 * Its first job is to confirm the pin map. The compass names come from a table
 * derived on paper by cross-referencing UM2750 and UM3062 by connector position;
 * this test is what turns that into a measurement, because the operator can see
 * whether the key they pressed is the one the firmware names.
 *
 * Takes no arguments, so it has no setup step: the scenario table carries `NULL`.
 */

#ifndef OTT_JOYSTICK_H
#define OTT_JOYSTICK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_joystick - public API
 * ========================================================================= */

bool ott_joystick_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_JOYSTICK_H */
