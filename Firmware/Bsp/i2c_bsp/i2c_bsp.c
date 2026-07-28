#include "i2c_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "i2c.h"

/* ==========================================================================
 * i2c_bsp - private
 * ========================================================================= */

/* The bus instance. I2C1 (PB8=SCL / PB9=SDA) serves mikroBUS slot 2; the handle
 * is brought up by the CubeMX MX_I2C1_Init(). Point this at another handle to
 * move the transport. */
#define I2C_BSP_HANDLE               (hi2c1)

/* Generous for a handful of bytes at ~100 kHz, but bounded so a dead bus is
 * reported in well under a second instead of hanging a test. */
#define I2C_BSP_TIMEOUT_MS           (100U)

/* The HAL takes the address already shifted into its 8-bit form. */
#define I2C_BSP_DEVICE_ADDRESS_SHIFT (1U)
#define I2C_BSP_DEVICE_ADDRESS_MAX   (0x7FU)

#define I2C_BSP_LENGTH_MAX           (UINT16_MAX)

static bool g_is_initialized = false;

static i2c_bsp_status_e prv_map_hal_status(HAL_StatusTypeDef in_status)
{
    if (in_status == HAL_OK)
    {
        return I2C_BSP_STATUS_OK;
    }

    if (in_status == HAL_TIMEOUT)
    {
        return I2C_BSP_STATUS_ERROR_TIMEOUT;
    }

    /* An acknowledge failure means the device NACKed — worth reporting apart
     * from a timeout, since it points at an absent or misaddressed device. */
    if ((HAL_I2C_GetError(&I2C_BSP_HANDLE) & HAL_I2C_ERROR_AF) != 0U)
    {
        return I2C_BSP_STATUS_ERROR_NOT_ACKNOWLEDGED;
    }

    return I2C_BSP_STATUS_ERROR_TIMEOUT;
}

static uint16_t prv_to_hal_device_address(uint8_t in_device_address)
{
    ASSERT(in_device_address <= I2C_BSP_DEVICE_ADDRESS_MAX);

    return (uint16_t)((uint16_t)in_device_address << I2C_BSP_DEVICE_ADDRESS_SHIFT);
}

static void prv_assert_transfer(const void* const in_data, size_t in_length)
{
    ASSERT(g_is_initialized);
    ASSERT(in_data != NULL);
    ASSERT(in_length > 0U);
    ASSERT(in_length <= I2C_BSP_LENGTH_MAX);
}

/* ==========================================================================
 * i2c_bsp - public
 * ========================================================================= */

void i2c_bsp_init(void)
{
    ASSERT(false == g_is_initialized);

    g_is_initialized = true;
}

i2c_bsp_status_e i2c_bsp_write(uint8_t in_device_address, const uint8_t* const in_data, size_t in_length)
{
    prv_assert_transfer(in_data, in_length);

    return prv_map_hal_status(HAL_I2C_Master_Transmit(&I2C_BSP_HANDLE, prv_to_hal_device_address(in_device_address),
                                                      (uint8_t*)in_data, (uint16_t)in_length, I2C_BSP_TIMEOUT_MS));
}

i2c_bsp_status_e i2c_bsp_read(uint8_t in_device_address, uint8_t* const out_data, size_t in_length)
{
    prv_assert_transfer(out_data, in_length);

    return prv_map_hal_status(HAL_I2C_Master_Receive(&I2C_BSP_HANDLE, prv_to_hal_device_address(in_device_address),
                                                     out_data, (uint16_t)in_length, I2C_BSP_TIMEOUT_MS));
}

i2c_bsp_status_e i2c_bsp_read_memory(uint8_t in_device_address, uint16_t in_memory_address,
                                     size_t in_memory_address_width, uint8_t* const out_data, size_t in_length)
{
    uint16_t hal_memory_address_size;

    prv_assert_transfer(out_data, in_length);

    if (in_memory_address_width == I2C_BSP_MEMORY_ADDRESS_WIDTH_8_BIT)
    {
        hal_memory_address_size = I2C_MEMADD_SIZE_8BIT;
    }
    else if (in_memory_address_width == I2C_BSP_MEMORY_ADDRESS_WIDTH_16_BIT)
    {
        hal_memory_address_size = I2C_MEMADD_SIZE_16BIT;
    }
    else
    {
        return I2C_BSP_STATUS_ERROR_UNSUPPORTED_WIDTH;
    }

    return prv_map_hal_status(HAL_I2C_Mem_Read(&I2C_BSP_HANDLE, prv_to_hal_device_address(in_device_address),
                                               in_memory_address, hal_memory_address_size, out_data,
                                               (uint16_t)in_length, I2C_BSP_TIMEOUT_MS));
}
