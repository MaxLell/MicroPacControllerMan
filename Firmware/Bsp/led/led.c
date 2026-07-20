#include "led.h"

#include "main.h" /* LED_GPIO_Port / LED_Pin + HAL (from the CubeMX export) */

void led_init(void)
{
    /* LD2 (PA5) is configured as a push-pull output by MX_GPIO_Init(); nothing
     * to do here. Kept for API symmetry with the other BSP modules. */
}

void led_set(int on)
{
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

int led_get(void)
{
    /* Reads the real pin level (IDR), even for an output pin. */
    return (HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

void led_toggle(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}
