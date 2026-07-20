#include "button.h"

#include "systick.h"

#include "main.h" /* USER_BUTTON_GPIO_Port / USER_BUTTON_Pin + HAL (CubeMX) */

void button_init(void)
{
    /* On this NUCLEO-G431RB, B1 is wired ACTIVE-HIGH: pressing drives PC13 to
     * VDD (measured 3.3 V pressed / ~2.4 V idle). CubeMX configures PC13 with a
     * pull-UP, which — fighting the board's external pull-down — biases the idle
     * level to ~2.4 V (already above V_IH), so a press cannot be distinguished.
     * Re-init PC13 with a pull-DOWN here so idle sits cleanly low. */
    GPIO_InitTypeDef init = {0};
    init.Pin = USER_BUTTON_Pin;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(USER_BUTTON_GPIO_Port, &init);
}

int button_pressed(void)
{
    /* Active-high: idle low (pull-down), pressed pulls the line up to VDD. */
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
