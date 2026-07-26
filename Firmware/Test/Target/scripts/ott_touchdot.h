#ifndef OTT_TOUCHDOT_H
#define OTT_TOUCHDOT_H

#include <stdint.h>

/* touchdot OTT: combined display + touchpad test. A dot on the LCD tracks the
 * finger position read from the touchpad, so the operator can confirm the two
 * devices work together and the coordinate mapping is right. Ends on a USER-button
 * press. */
int ott_touchdot_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_touchdot_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_TOUCHDOT_H */
