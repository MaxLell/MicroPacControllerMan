#include "led.h"

#include "stm32g4xx.h"

#define LED_PIN 5U /* PA5 = LD2 */

void led_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER &= ~GPIO_MODER_MODE5_Msk;
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODE5_Pos); /* output */
}

void led_set(int on)
{
    /* BSRR: atomic set (low half) / reset (high half) */
    GPIOA->BSRR = on ? (1U << LED_PIN) : (1U << (LED_PIN + 16U));
}

int led_get(void)
{
    /* IDR reflects the real pin level, even for an output pin. */
    return (int)((GPIOA->IDR >> LED_PIN) & 1U);
}

void led_toggle(void)
{
    GPIOA->ODR ^= (1U << LED_PIN);
}
