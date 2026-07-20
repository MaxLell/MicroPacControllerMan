#include "button.h"

#include "systick.h"

#include "main.h" /* USER_BUTTON_GPIO_Port / USER_BUTTON_Pin + HAL (CubeMX) */

void button_init(void)
{
    /* PC13 (B1) is configured as an input with pull-up by MX_GPIO_Init(); nothing
     * to do here. Kept for API symmetry with the other BSP modules. */
}

int button_pressed(void)
{
    /* Active-low: idle high (pull-up), pressed pulls the line low. */
    return (HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin) == GPIO_PIN_RESET) ? 1 : 0;
}

int button_wait_press(unsigned timeout_ms)
{
    uint32_t start = millis();

    /* Require the button to start released so a stale hold from a prior test does
     * not count. */
    while (button_pressed()) {
        if ((millis() - start) >= timeout_ms) {
            return 0;
        }
    }

    /* Wait for a press, debounced by ~20 ms of stable low. */
    for (;;) {
        if ((millis() - start) >= timeout_ms) {
            return 0;
        }
        if (button_pressed()) {
            delay_ms(20);
            if (button_pressed()) {
                return 1;
            }
        }
    }
}
