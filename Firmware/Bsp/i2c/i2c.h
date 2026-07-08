#ifndef I2C_H
#define I2C_H

#include <stddef.h>
#include <stdint.h>

/*
 * I2C1 transport for mikroBUS slot 2 (Touchpad Click, MTCH6102).
 *
 * Pins (R-001, derived; to be confirmed on hardware):
 *   SCL = PB8 (AF4)
 *   SDA = PB9 (AF4)
 * 100 kHz standard mode; the Click board carries its own SCL/SDA pull-ups
 * (internal pull-ups are also enabled as a safety net).
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

/* Write wlen bytes (e.g. a register pointer) then repeated-START read rlen bytes. */
int i2c_write_read(uint8_t addr7, const uint8_t* wr, size_t wlen, uint8_t* rd, size_t rlen);

#endif /* I2C_H */
