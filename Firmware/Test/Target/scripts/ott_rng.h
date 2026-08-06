/*
 * ott_rng.h
 *
 * rng OTT: the MCU's random number generator, on real silicon (FR-045, VT-INT-028).
 *
 * There are two failures worth separating, and only hardware can tell them apart.
 *
 *  - The generator never came up. HSI48 is its kernel clock and nothing else in this firmware uses
 *    it, so a missing clock enable leaves the peripheral enabled and permanently not ready — which
 *    reads as a game whose timings never vary, and looks like a design rather than a fault.
 *  - It came up and produces the *same* word every time, or zero. That is what a host stub looks
 *    like, and it is what a peripheral with a dead entropy source looks like too.
 *
 * So this test asks for a handful of words and checks they are neither constant nor all zero. It
 * cannot check that they are *random* — no test of a few words can, and a statistical battery is not
 * this project's business — but "not the same number twice" is the property the game depends on and
 * it is the property a fault would break.
 *
 * Automatic: nothing is drawn and nobody has to judge anything. Takes no arguments, so the scenario
 * table carries `NULL`.
 */

#ifndef OTT_RNG_H
#define OTT_RNG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_rng - public API
 * ========================================================================= */

bool ott_rng_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_RNG_H */
