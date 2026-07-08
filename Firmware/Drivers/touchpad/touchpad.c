#include "touchpad.h"

#include "i2c.h"
#include "systick.h"

#include "stm32g4xx.h"

#define RST_PIN 4U /* PA4 */

/* MTCH6102 register map (DS40001750): the touch block auto-increments. */
#define REG_FW_MAJOR  0x00U
#define REG_TOUCHSTATE 0x10U /* [0]=TCH present; then TOUCHX, TOUCHY, TOUCHLSB */

#define TOUCHSTATE_TCH 0x01U

void touchpad_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* RST (PA4) as output, driven high to release the controller from reset. */
    GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODE4_Pos);
    GPIOA->BSRR = (1U << (RST_PIN + 16U)); /* assert reset (low) */
    delay_ms(5);
    GPIOA->BSRR = (1U << RST_PIN); /* release */
    delay_ms(50);                  /* controller boot time */

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
