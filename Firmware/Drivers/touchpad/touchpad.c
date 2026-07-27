#include "touchpad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "delay.h"
#include "dio_bsp.h"
#include "i2c_bsp.h"

/* ==========================================================================
 * touchpad - private
 * ========================================================================= */

#define TOUCHPAD_DEVICE_ADDRESS (0x25U)

/* MTCH6102 register map (DS40001750). */
#define TOUCHPAD_REGISTER_FIRMWARE_MAJOR (0x00U)
#define TOUCHPAD_REGISTER_TOUCH_STATE (0x10U)

#define TOUCHPAD_TOUCH_STATE_TOUCH_PRESENT (0x01U)

#define TOUCHPAD_FIRMWARE_VERSION_LENGTH (2U)

/* The touch block auto-increments from the touch-state register. */
#define TOUCHPAD_TOUCH_BLOCK_LENGTH (4U)
#define TOUCHPAD_TOUCH_STATE_INDEX (0U)
#define TOUCHPAD_TOUCH_X_INDEX (1U)
#define TOUCHPAD_TOUCH_Y_INDEX (2U)
#define TOUCHPAD_TOUCH_LSB_INDEX (3U)

/* Each axis is a byte of high bits plus a nibble in the shared LSB register: the
 * X nibble sits in the upper half, the Y nibble in the lower half. */
#define TOUCHPAD_LSB_NIBBLE_BITS (4U)
#define TOUCHPAD_LSB_X_SHIFT (4U)
#define TOUCHPAD_LSB_Y_MASK (0x0FU)

#define TOUCHPAD_RESET_PULSE_MS (5U)
#define TOUCHPAD_BOOT_MS (50U)

/* ==========================================================================
 * touchpad - public
 * ========================================================================= */

void touchpad_init(void)
{
    /* Reset is active LOW: pulse it, then let the controller boot. */
    dio_bsp_set_pin(DIO_BSP_PIN_TOUCHPAD_RESET, DIO_BSP_PIN_STATE_LOW);
    delay_ms(TOUCHPAD_RESET_PULSE_MS);
    dio_bsp_set_pin(DIO_BSP_PIN_TOUCHPAD_RESET, DIO_BSP_PIN_STATE_HIGH);
    delay_ms(TOUCHPAD_BOOT_MS);
}

i2c_bsp_status_e touchpad_probe(void)
{
    uint8_t firmware_version[TOUCHPAD_FIRMWARE_VERSION_LENGTH];

    return i2c_bsp_read_memory(TOUCHPAD_DEVICE_ADDRESS, TOUCHPAD_REGISTER_FIRMWARE_MAJOR,
                               I2C_BSP_MEMORY_ADDRESS_WIDTH_8_BIT, firmware_version,
                               sizeof(firmware_version));
}

i2c_bsp_status_e touchpad_read(touchpad_reading_t* out_reading)
{
    uint8_t touch_block[TOUCHPAD_TOUCH_BLOCK_LENGTH];
    i2c_bsp_status_e status;

    ASSERT(out_reading != NULL);

    out_reading->x = 0U;
    out_reading->y = 0U;
    out_reading->is_touched = false;

    status = i2c_bsp_read_memory(TOUCHPAD_DEVICE_ADDRESS, TOUCHPAD_REGISTER_TOUCH_STATE,
                                 I2C_BSP_MEMORY_ADDRESS_WIDTH_8_BIT, touch_block,
                                 sizeof(touch_block));
    if (status != I2C_BSP_STATUS_OK)
    {
        return status;
    }

    out_reading->is_touched
        = (touch_block[TOUCHPAD_TOUCH_STATE_INDEX] & TOUCHPAD_TOUCH_STATE_TOUCH_PRESENT) != 0U;

    if (out_reading->is_touched)
    {
        out_reading->x = (uint16_t)(
            ((uint16_t)touch_block[TOUCHPAD_TOUCH_X_INDEX] << TOUCHPAD_LSB_NIBBLE_BITS)
            | (touch_block[TOUCHPAD_TOUCH_LSB_INDEX] >> TOUCHPAD_LSB_X_SHIFT));

        out_reading->y = (uint16_t)(
            ((uint16_t)touch_block[TOUCHPAD_TOUCH_Y_INDEX] << TOUCHPAD_LSB_NIBBLE_BITS)
            | (touch_block[TOUCHPAD_TOUCH_LSB_INDEX] & TOUCHPAD_LSB_Y_MASK));
    }

    return I2C_BSP_STATUS_OK;
}
