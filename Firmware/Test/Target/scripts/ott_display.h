/*
 * ott_display.h
 *
 * display OTT (VT-INT-006): renders a sequence of geometric test patterns (lines,
 * rectangles, circles, triangles) on the panel so the operator can confirm it
 * works. Cycles the scenes, then holds a composite until the user button is
 * pressed.
 */

#ifndef OTT_DISPLAY_H
#define OTT_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_display - public API
 * ========================================================================= */

bool ott_display_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                       uint32_t* out_parameter_size);

bool ott_display_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason, size_t in_reason_size);

#endif /* OTT_DISPLAY_H */
