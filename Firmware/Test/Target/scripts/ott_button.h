#ifndef OTT_BUTTON_H
#define OTT_BUTTON_H

#include <stdint.h>

/* button OTT (board bring-up): streams the live USER-button state (B1 = PC13)
 * over the serial console — a per-second heartbeat plus every debounced edge —
 * so the operator can see whether the pin actually toggles. Passes once three
 * presses have been seen; fails on a 30 s timeout with a diagnostic reason. */
int ott_button_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_button_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_BUTTON_H */
