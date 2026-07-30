#include "spi_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "spi.h"

/* ==========================================================================
 * spi_bsp - private
 * ========================================================================= */

/* The bus instance. SPI1 serves the X-NUCLEO-GFX01M2 display as SCK=PA5 /
 * MOSI=PA7 / MISO=PA6 (AF5), full-duplex master, mode 0, 8-bit, MSB-first. The
 * handle is brought up by the CubeMX MX_SPI1_Init(). Point this at another handle
 * to move the bus. */
#define SPI_BSP_HANDLE (hspi1)

/* Bounded so a stuck bus is reported rather than hanging a test. Generous even for
 * a full 240x320 RGB565 frame: 153.6 kB at the configured 5 Mbit/s is ~250 ms, and
 * the driver sends far smaller pieces than that. */
#define SPI_BSP_TIMEOUT_MS (500U)

#define SPI_BSP_LENGTH_MAX (UINT16_MAX)

static bool g_is_initialized = false;

/* ==========================================================================
 * spi_bsp - public
 * ========================================================================= */

void spi_bsp_init(void)
{
    ASSERT(false == g_is_initialized);

    g_is_initialized = true;
}

void spi_bsp_write(const uint8_t* const in_data, size_t in_length)
{
    ASSERT(g_is_initialized);
    ASSERT(in_data != NULL);
    ASSERT(in_length > 0U);
    ASSERT(in_length <= SPI_BSP_LENGTH_MAX);

    (void)HAL_SPI_Transmit(&SPI_BSP_HANDLE, (uint8_t*)in_data, (uint16_t)in_length,
                           SPI_BSP_TIMEOUT_MS);
}

void spi_bsp_transfer(const uint8_t* const in_tx_data, uint8_t* out_rx_data, size_t in_length)
{
    ASSERT(g_is_initialized);
    ASSERT(in_tx_data != NULL);
    ASSERT(out_rx_data != NULL);
    ASSERT(in_length > 0U);
    ASSERT(in_length <= SPI_BSP_LENGTH_MAX);

    (void)HAL_SPI_TransmitReceive(&SPI_BSP_HANDLE, (uint8_t*)in_tx_data, out_rx_data,
                                  (uint16_t)in_length, SPI_BSP_TIMEOUT_MS);
}
