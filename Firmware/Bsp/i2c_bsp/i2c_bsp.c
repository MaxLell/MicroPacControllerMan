#include "bsp_i2c.h"

#include "main.h" /* HAL types (from the CubeMX export) */

/* Defined in the CubeMX-generated i2c.c. Referenced by extern so we don't include
 * the generated Core/Inc/i2c.h (kept off this module's include story on purpose). */
extern I2C_HandleTypeDef hi2c1;

/* Generous vs. a few bytes at ~100 kHz, but bounded so a dead bus is reported in
 * well under a second (HAL timeouts are in milliseconds, off the HAL tick). */
#define I2C_TIMEOUT_MS (100U)

/* Map a HAL status to the BSP return codes. On HAL_ERROR, an acknowledge failure
 * (AF) means the device NACKed — surface that distinctly for the OTT reports. */
static int prv_status(HAL_StatusTypeDef st)
{
    if (st == HAL_OK) {
        return I2C_OK;
    }
    if (st == HAL_TIMEOUT) {
        return I2C_ERR_TIMEOUT;
    }
    if (HAL_I2C_GetError(&hi2c1) & HAL_I2C_ERROR_AF) {
        return I2C_ERR_NACK;
    }
    return I2C_ERR_TIMEOUT;
}

void i2c_init(void)
{
    /* I2C1 (PB8/PB9) is brought up by MX_I2C1_Init(); nothing to do here. */
}

int i2c_write(uint8_t addr7, const uint8_t* data, size_t len)
{
    return prv_status(HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr7 << 1), (uint8_t*)data,
                                              (uint16_t)len, I2C_TIMEOUT_MS));
}

int i2c_read(uint8_t addr7, uint8_t* data, size_t len)
{
    return prv_status(
        HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(addr7 << 1), data, (uint16_t)len, I2C_TIMEOUT_MS));
}

int i2c_write_read(uint8_t addr7, const uint8_t* wr, size_t wlen, uint8_t* rd, size_t rlen)
{
    /* Register-pointer write + repeated-START read = exactly HAL_I2C_Mem_Read.
     * All MTCH6102 registers use a 1-byte pointer; a 2-byte pointer is also
     * supported. */
    uint16_t mem_addr;
    uint16_t mem_size;
    if (wlen == 1U) {
        mem_addr = wr[0];
        mem_size = I2C_MEMADD_SIZE_8BIT;
    } else if (wlen == 2U) {
        mem_addr = (uint16_t)(((uint16_t)wr[0] << 8) | wr[1]);
        mem_size = I2C_MEMADD_SIZE_16BIT;
    } else {
        return I2C_ERR_TIMEOUT; /* unsupported pointer width */
    }
    return prv_status(HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), mem_addr, mem_size, rd,
                                       (uint16_t)rlen, I2C_TIMEOUT_MS));
}
