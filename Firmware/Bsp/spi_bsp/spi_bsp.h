/*
 * spi_bsp.h
 *
 * Blocking SPI master transport. Chip-select is a plain digital output owned by
 * the device driver, not by this module, because the framing differs per device —
 * and on the GFX01M2 the display's chip-select is active *high*, which no hardware
 * NSS would produce.
 *
 * The SPI instance and the bus parameters are `#define`s in spi_bsp.c.
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
 * The peripheral itself is brought up by the CubeMX MX_SPI1_Init() before the
 * firmware entry point, so this only resets the module state. Must be called
 * exactly once, before any other function of this module.
 */
void spi_bsp_init(void);

/*! \brief Send a byte sequence, blocking until the transfer has finished.
 *
 * What a display driver uses for commands and pixel data: the reply is
 * meaningless there, so nothing is captured.
 *
 * \param[in]       in_data: bytes to send, must not be `NULL`
 * \param[in]       in_length: number of bytes in `in_data`, at least `1`
 */
void spi_bsp_write(const uint8_t* const in_data, size_t in_length);

/*! \brief Exchange a byte sequence, blocking until the transfer has finished.
 *
 * Full duplex: `in_length` bytes go out while `in_length` bytes come back. Needed
 * to read a device register — which is how a driver can identify the controller it
 * is talking to instead of trusting a label on the board.
 *
 * \param[in]       in_tx_data: bytes to send, must not be `NULL`
 * \param[out]      out_rx_data: receives the same number of bytes, must not be
 *                      `NULL`. May not overlap `in_tx_data`.
 * \param[in]       in_length: number of bytes to exchange, at least `1`
 */
void spi_bsp_transfer(const uint8_t* const in_tx_data, uint8_t* out_rx_data, size_t in_length);

#endif /* SPI_BSP_H */
