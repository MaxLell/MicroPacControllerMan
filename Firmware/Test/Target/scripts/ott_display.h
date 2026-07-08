#ifndef OTT_DISPLAY_H
#define OTT_DISPLAY_H

#include <stdint.h>

/* display OTT (VT-INT-006): renders a sequence of geometric test patterns
 * (lines, rectangles, circles, triangles — Adafruit-GFX style, no logo) on the
 * LCD Mono Click so the operator can confirm the panel works. Cycles the scenes,
 * then holds a composite until the USER button is pressed. */
int ott_display_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_display_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_DISPLAY_H */
