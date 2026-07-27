#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stddef.h>
#include <stdint.h>

/*
 * I2C1 transport for mikroBUS slot 2 (Touchpad Click, MTCH6102).
 *
 * Thin wrapper over the STM32 HAL I2C1 instance (hi2c1, brought up by the CubeMX
 * MX_I2C1_Init on PB8=SCL / PB9=SDA). Named bsp_i2c to avoid a clash with the
 * CubeMX-generated Core/Inc/i2c.h on the include path.
 *
 * All calls take a 7-bit address, are blocking with a bounded timeout, and return
 * 0 on success or a negative code on NACK/timeout — so a missing/miswired device
 * fails cleanly instead of hanging (important for the OTT reports).
 */

#define I2C_OK        (0)
#define I2C_ERR_NACK  (-1)
#define I2C_ERR_TIMEOUT (-2)

void i2c_init(void);

/* Write len bytes to addr7. */
int i2c_write(uint8_t addr7, const uint8_t* data, size_t len);

/* Read len bytes from addr7. */
int i2c_read(uint8_t addr7, uint8_t* data, size_t len);

/* Write wlen bytes (a 1- or 2-byte register pointer) then repeated-START read
 * rlen bytes. */
int i2c_write_read(uint8_t addr7, const uint8_t* wr, size_t wlen, uint8_t* rd, size_t rlen);

#endif /* BSP_I2C_H */
