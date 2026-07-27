#ifndef BSP_SPI_H
#define BSP_SPI_H

#include <stddef.h>
#include <stdint.h>

/*
 * SPI1 transport for mikroBUS slot 1 (LCD Mono Click, Sharp LS013B7DH03).
 *
 * Thin wrapper over the STM32 HAL SPI1 instance (hspi1, brought up by CubeMX
 * MX_SPI1_Init): PA5=SCK, PA7=MOSI (AF5), transmit-only master, mode 0, 8-bit,
 * LSB-first, ~0.66 MHz (panel max ~1.1 MHz). MISO/PA6 is NOT used by SPI — the
 * Click board repurposes that mikroBUS line as the display's DISP control.
 *
 * The panel is write-only, so only spi_write() is provided. Chip-select is an
 * active-HIGH GPIO driven by the display driver, not by this module. Named
 * bsp_spi to avoid a clash with the CubeMX-generated Core/Inc/spi.h.
 */

void spi_init(void);
void spi_write(const uint8_t* data, size_t len);

#endif /* BSP_SPI_H */
