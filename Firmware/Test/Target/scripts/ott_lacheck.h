#ifndef OTT_LACHECK_H
#define OTT_LACHECK_H

#include <stdint.h>

/* lacheck OTT (M2 board bring-up): logic-analyzer wiring / pinout check for the
 * LCD Mono Click SPI signals. Drives each line with a UNIQUE, self-identifying
 * fingerprint so a capture reveals which probe sits on which pin (and catches
 * swapped or misplaced probes) BEFORE trusting any dispdiag/display capture:
 *
 *   CS       (PB6)  -> 2 slow GPIO pulses
 *   DISP     (PA6)  -> 3 slow GPIO pulses
 *   EXTCOMIN (PB10) -> 4 slow GPIO pulses
 *   SCK      (PA5)  -> fast clock burst in EVERY SPI burst
 *   MOSI     (PA7)  -> flat within a burst; HIGH only on the 0xFF bursts
 *
 * One sweep = CS, DISP, EXTCOMIN pulse groups (separated by gaps), then 6 spaced
 * SPI bursts with alternating 0x00/0xFF data, then a long sync gap. The sweep
 * repeats until the USER button (B1) is pressed (or a safety cap). */
int ott_lacheck_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);
int ott_lacheck_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

#endif /* OTT_LACHECK_H */
