#include "touchpad.h"

#include "bsp_i2c.h"
#include "systick.h"

#include "main.h" /* TOUCH_RST_GPIO_Port / TOUCH_RST_Pin + HAL (from the CubeMX export) */

/* MTCH6102 register map (DS40001750): the touch block auto-increments. */
#define REG_FW_MAJOR  0x00U
#define REG_TOUCHSTATE 0x10U /* [0]=TCH present; then TOUCHX, TOUCHY, TOUCHLSB */

#define TOUCHSTATE_TCH 0x01U

void touchpad_init(void)
{
    /* RST (PA4) is a GPIO output configured by MX_GPIO_Init(). Pulse it low to
     * assert reset, then release high and let the controller boot. */
    HAL_GPIO_WritePin(TOUCH_RST_GPIO_Port, TOUCH_RST_Pin, GPIO_PIN_RESET);
    delay_ms(5);
    HAL_GPIO_WritePin(TOUCH_RST_GPIO_Port, TOUCH_RST_Pin, GPIO_PIN_SET);
    delay_ms(50); /* controller boot time */

    i2c_init();
}

int touchpad_probe(void)
{
    uint8_t reg = REG_FW_MAJOR;
    uint8_t id[2];
    return i2c_write_read(TOUCHPAD_ADDR7, &reg, 1, id, sizeof(id));
}

int touchpad_read(uint16_t* x, uint16_t* y, int* touched)
{
    uint8_t reg = REG_TOUCHSTATE;
    uint8_t buf[4]; /* TOUCHSTATE, TOUCHX, TOUCHY, TOUCHLSB */

    int rc = i2c_write_read(TOUCHPAD_ADDR7, &reg, 1, buf, sizeof(buf));
    if (rc != I2C_OK) {
        return rc;
    }

    *touched = (buf[0] & TOUCHSTATE_TCH) ? 1 : 0;
    if (*touched) {
        *x = (uint16_t)(((uint16_t)buf[1] << 4) | (buf[3] >> 4));
        *y = (uint16_t)(((uint16_t)buf[2] << 4) | (buf[3] & 0x0FU));
    }
    return I2C_OK;
}
