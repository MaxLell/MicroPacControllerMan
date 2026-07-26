#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <stdint.h>

/*
 * Touchpad Click driver — Microchip MTCH6102 capacitive controller over I2C1
 * (Bsp/i2c), mikroBUS slot 2. The controller boots in Full mode with the default
 * 9x6 channel map, so raw X/Y position is available with no configuration writes.
 *
 * Pins (Click Shield for Nucleo-64 / ST-Morpho, HW-confirmed): SCL=PB8, SDA=PB9.
 * RST is driven high on PA4; the shield actually routes slot-2 RST to PD2, but the
 * MTCH6102 boots without an explicit reset so this is cosmetic. INT (shield PC9)
 * is left unused — position is polled. (Do NOT confuse with PB3, now SPI1_SCK.)
 *
 * Coordinates are the controller's raw decoded position: X in 0..TOUCHPAD_X_MAX,
 * Y in 0..TOUCHPAD_Y_MAX. This is what FR-004's 4-quadrant control maps from.
 */

#define TOUCHPAD_ADDR7 0x25 /* MTCH6102 default 7-bit I2C address */
#define TOUCHPAD_X_MAX 576  /* 9 X-channels * 64 */
#define TOUCHPAD_Y_MAX 384  /* 6 Y-channels * 64 */

void touchpad_init(void);

/* Confirm the MTCH6102 acknowledges on the bus. Returns 0 on success. */
int touchpad_probe(void);

/* Read the current touch. Sets the touched flag (1 if a finger is present) and,
 * when touched, the x/y outputs to the raw position. Returns 0 on success,
 * negative on I2C error. */
int touchpad_read(uint16_t* x, uint16_t* y, int* touched);

#endif /* TOUCHPAD_H */
