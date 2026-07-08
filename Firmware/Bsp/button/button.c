#include "button.h"

#include "systick.h"

#include "stm32g4xx.h"

#define BTN_PIN 13U /* PC13 = B1 */

void button_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
    GPIOC->MODER &= ~GPIO_MODER_MODE13_Msk; /* input */
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD13_Msk;
    GPIOC->PUPDR |= (0x1U << GPIO_PUPDR_PUPD13_Pos); /* pull-up: idle high, pressed low */
}

int button_pressed(void)
{
    return ((GPIOC->IDR >> BTN_PIN) & 1U) == 0U;
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
