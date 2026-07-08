#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>

/*
 * SPI1 transport for mikroBUS slot 1 (LCD Mono Click).
 *
 * Pins (R-001, derived from the Click Shield for Nucleo-64 → Arduino → G431 map,
 * to be confirmed on hardware):
 *   SCK  = PA5 (AF5)   — shared with the on-board LED LD2
 *   MOSI = PA7 (AF5)
 *   MISO = PA6         — NOT used for SPI; the LCD Mono Click repurposes this
 *                        mikroBUS line as the display's DISP control (see display.c)
 *
 * The Sharp LS013B7DH03 is a write-only display, so SPI runs transmit-only.
 * Format: mode 0 (CPOL=0, CPHA=0), 8-bit, LSB-first, ~1 MHz (LS013B7DH03 max 1.1 MHz).
 * Chip-select is active-HIGH on this panel and is driven as a GPIO by the display
 * driver, not by this module.
 */

void spi_init(void);
void spi_write(const uint8_t* data, size_t len);

#endif /* SPI_H */
