/*
 * ott_display_test.h
 *
 * display_test OTT (board bring-up): brings the panel up and draws patterns a person can
 * judge. It checks what the firmware can check by itself — that the controller
 * identifies as an ST7789V — and then shows what it cannot: whether the colours are
 * right, which corner is the origin, and whether the geometry matches 240x320.
 *
 * The operator confirms with B1, as with the other visual tests.
 */

#ifndef OTT_DISPLAY_TEST_H
#define OTT_DISPLAY_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_display_test - public API
 * ========================================================================= */

bool ott_display_test_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_DISPLAY_TEST_H */
