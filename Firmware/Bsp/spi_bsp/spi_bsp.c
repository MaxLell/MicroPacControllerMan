#include "spi_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "spi.h"

/* ==========================================================================
 * spi_bsp - private
 * ========================================================================= */

/* The bus instance. SPI1 serves mikroBUS slot 1 as SCK=PB3 / MOSI=PB5 (AF5),
 * transmit-only master, mode 0, 8-bit, LSB-first, ~0.66 MHz. The handle is
 * brought up by the CubeMX MX_SPI1_Init(). MISO is deliberately absent: the LCD
 * Mono Click repurposes that mikroBUS line as the panel's DISP control, which is
 * a plain digital output (see dio_bsp). */
#define SPI_BSP_HANDLE     (hspi1)

/* Generous for a 128-byte display line at ~0.66 MHz (~1.6 ms), but bounded so a
 * stuck bus is reported rather than hanging a test. */
#define SPI_BSP_TIMEOUT_MS (100U)

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

    (void)HAL_SPI_Transmit(&SPI_BSP_HANDLE, (uint8_t*)in_data, (uint16_t)in_length, SPI_BSP_TIMEOUT_MS);
}
