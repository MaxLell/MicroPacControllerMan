#ifndef OTT_TOUCHPAD_H
#define OTT_TOUCHPAD_H

#include <stdint.h>

/* touchpad OTT (VT-INT-007): continuously prints the live MTCH6102 touch state
 * (x, y, touch-present) over the serial console so the operator can confirm the
 * reading follows their finger. Ends on a USER-button press. */
int ott_touchpad_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_touchpad_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_TOUCHPAD_H */
