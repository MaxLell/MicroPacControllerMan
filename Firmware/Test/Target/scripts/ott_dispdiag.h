#ifndef OTT_DISPDIAG_H
#define OTT_DISPDIAG_H

#include <stdint.h>

/* dispdiag OTT (M2 board bring-up, blank-panel diagnosis): a logic-analyzer-
 * oriented probe for the LCD Mono Click (LS013B7DH03 / SPI1). Unlike the
 * `display` scenario it draws no geometry — it drives deterministic, easy-to-
 * decode signals and narrates each phase over the VCP so a capture can be
 * correlated:
 *   1. static levels   — DISP/CS/EXTCOMIN held static so DC levels can be metered
 *   2. short bursts    — repeated 2-byte all-clear commands ([0x04][0x00]) to
 *                        decode CS/SCK/MOSI framing, SPI mode and LSB bit order
 *   3. full frames     — alternating all-black / all-white flushes (bulk data +
 *                        the actual visual max-contrast test)
 * Runs until the USER button (B1) is pressed. */
int ott_dispdiag_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_dispdiag_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_DISPDIAG_H */
