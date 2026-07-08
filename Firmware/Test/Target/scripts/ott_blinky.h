#ifndef OTT_BLINKY_H
#define OTT_BLINKY_H

#include <stdint.h>

/* blinky OTT: verifies the LED (PA5) can be driven and the pin follows,
 * by reading the level back from the input register (VT-INT-005). */
int ott_blinky_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_blinky_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_BLINKY_H */
