/*
 * ott_disptest.h
 *
 * disptest OTT (board bring-up): brings the panel up and draws patterns a person can
 * judge. It checks what the firmware can check by itself — that the controller
 * identifies as an ST7789V — and then shows what it cannot: whether the colours are
 * right, which corner is the origin, and whether the geometry matches 240x320.
 *
 * The operator confirms with B1, as with the other visual tests.
 */

#ifndef OTT_DISPTEST_H
#define OTT_DISPTEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_disptest - public API
 * ========================================================================= */

bool ott_disptest_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_DISPTEST_H */
