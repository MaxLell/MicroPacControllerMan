/*
 * ott_blinky.h
 *
 * blinky OTT (board bring-up): drives the on-board LED and reads the pin back to
 * confirm it actually followed. This is the project's first **automatic** OTT — the
 * pin is both written and read through `dio_bsp`, so the firmware can judge itself
 * and nobody has to watch the board or confirm anything.
 *
 * On success it then blinks the LED visibly for about a second, so a person who
 * happens to be looking gets a confirmation too.
 *
 * Takes no arguments, so it has no setup step: the scenario table carries `NULL`.
 */

#ifndef OTT_BLINKY_H
#define OTT_BLINKY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_blinky - public API
 * ========================================================================= */

bool ott_blinky_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_BLINKY_H */
