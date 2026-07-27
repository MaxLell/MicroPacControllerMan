/*
 * spi_bsp.h
 *
 * Blocking transmit-only SPI master transport. Chip-select is a plain digital
 * output owned by the device driver, not by this module, because the framing
 * differs per device. The SPI instance is a single #define in spi_bsp.c.
 */

#ifndef SPI_BSP_H
#define SPI_BSP_H

#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * spi_bsp - public API
 * ========================================================================= */

/*! \brief Initialize the SPI transport.
 *
 * The peripheral itself is brought up by the CubeMX MX_SPI1_Init() before
 * app_main(), so this only resets the module state.
 */
void spi_bsp_init(void);

/*! \brief Send a byte sequence, blocking until the transfer has finished.
 *
 * \param[in]       in_data: bytes to send, must not be `NULL`
 * \param[in]       in_length: number of bytes in `in_data`, at least `1`
 */
void spi_bsp_write(const uint8_t* const in_data, size_t in_length);

#endif /* SPI_BSP_H */
