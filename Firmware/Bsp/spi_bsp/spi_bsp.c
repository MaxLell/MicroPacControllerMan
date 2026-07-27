#include "bsp_spi.h"

#include "main.h" /* HAL types (from the CubeMX export) */

/* Defined in the CubeMX-generated spi.c (same name as our BSP header, so we
 * reference the handle by extern rather than including the generated spi.h). */
extern SPI_HandleTypeDef hspi1;

/* Generous vs. a 128-byte line at ~0.66 MHz (~1.6 ms), but bounded so a stuck
 * bus is reported rather than hanging. */
#define SPI_TIMEOUT_MS (100U)

void spi_init(void)
{
    /* SPI1 (PA5=SCK / PA7=MOSI) is brought up by MX_SPI1_Init(); nothing to do. */
}

void spi_write(const uint8_t* data, size_t len)
{
    /* Blocking, transmit-only; HAL waits for the transfer (and BSY) to finish. */
    HAL_SPI_Transmit(&hspi1, (uint8_t*)data, (uint16_t)len, SPI_TIMEOUT_MS);
}
