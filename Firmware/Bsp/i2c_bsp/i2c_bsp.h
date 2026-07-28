/*
 * i2c_bsp.h
 *
 * Blocking I2C master transport. Device-agnostic: callers pass a 7-bit device
 * address, every call has a bounded timeout, and a missing or miswired device
 * reports an error instead of hanging. The I2C instance is a single #define in
 * i2c_bsp.c.
 */

#ifndef I2C_BSP_H
#define I2C_BSP_H

#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * i2c_bsp - public types
 * ========================================================================= */

/*! \brief Width of a device's internal memory-address pointer, in bytes. */
#define I2C_BSP_MEMORY_ADDRESS_WIDTH_8_BIT  (1U)
#define I2C_BSP_MEMORY_ADDRESS_WIDTH_16_BIT (2U)

typedef enum
{
    I2C_BSP_STATUS_OK = 0,                 /*!< Transfer completed                       */
    I2C_BSP_STATUS_ERROR_NOT_ACKNOWLEDGED, /*!< Device did not acknowledge (absent?)     */
    I2C_BSP_STATUS_ERROR_TIMEOUT,          /*!< Bus did not complete within the timeout  */
    I2C_BSP_STATUS_ERROR_UNSUPPORTED_WIDTH /*!< Memory-address width is not supported    */
} i2c_bsp_status_e;

/* ==========================================================================
 * i2c_bsp - public API
 * ========================================================================= */

/*! \brief Initialize the I2C transport.
 *
 * The peripheral itself is brought up by the CubeMX MX_I2C1_Init() before
 * app_main(), so this only resets the module state.
 */
void i2c_bsp_init(void);

/*! \brief Write a byte sequence to a device.
 *
 * \param[in]       in_device_address: 7-bit device address
 * \param[in]       in_data: bytes to send, must not be `NULL`
 * \param[in]       in_length: number of bytes in `in_data`, at least `1`
 * \return          \ref I2C_BSP_STATUS_OK on success, member of
 *                      \ref i2c_bsp_status_e otherwise
 */
i2c_bsp_status_e i2c_bsp_write(uint8_t in_device_address, const uint8_t* const in_data, size_t in_length);

/*! \brief Read a byte sequence from a device.
 *
 * \param[in]       in_device_address: 7-bit device address
 * \param[out]      out_data: receives the bytes, must not be `NULL`
 * \param[in]       in_length: number of bytes to read, at least `1`
 * \return          \ref I2C_BSP_STATUS_OK on success, member of
 *                      \ref i2c_bsp_status_e otherwise
 */
i2c_bsp_status_e i2c_bsp_read(uint8_t in_device_address, uint8_t* const out_data, size_t in_length);

/*! \brief Read from a device's internal memory.
 *
 * Writes the memory address, then reads `in_length` bytes after a repeated START
 * — the usual register-read sequence of an I2C peripheral chip.
 *
 * \param[in]       in_device_address: 7-bit device address
 * \param[in]       in_memory_address: address inside the device to read from
 * \param[in]       in_memory_address_width: \ref I2C_BSP_MEMORY_ADDRESS_WIDTH_8_BIT
 *                      or \ref I2C_BSP_MEMORY_ADDRESS_WIDTH_16_BIT
 * \param[out]      out_data: receives the bytes, must not be `NULL`
 * \param[in]       in_length: number of bytes to read, at least `1`
 * \return          \ref I2C_BSP_STATUS_OK on success, member of
 *                      \ref i2c_bsp_status_e otherwise
 */
i2c_bsp_status_e i2c_bsp_read_memory(uint8_t in_device_address, uint16_t in_memory_address,
                                     size_t in_memory_address_width, uint8_t* const out_data, size_t in_length);

#endif /* I2C_BSP_H */
