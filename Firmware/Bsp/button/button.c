#include "button.h"

#include "systick.h"

#include "main.h" /* USER_BUTTON_GPIO_Port / USER_BUTTON_Pin + HAL (CubeMX) */

void button_init(void)
{
    /* PC13 is configured by MX_GPIO_Init() as an input with no pull (GPIO_NOPULL
     * in the .ioc); the board's external pull-down holds the idle level low.
     * Nothing to do here — the pull now lives in CubeMX (single source). */
}

int button_pressed(void)
{
    /* This board's B1 is ACTIVE-HIGH: idle low (external pull-down), pressing
     * drives PC13 to VDD (measured 3.3 V pressed). This polarity must live in
     * firmware — CubeMX cannot express it. */
    return (HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin) == GPIO_PIN_SET) ? 1 : 0;
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
