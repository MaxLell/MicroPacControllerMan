/*
 * MicroPacControllerMan — Milestone 1: Toolchain Bring-Up
 *
 * Proves the build/flash/run chain on the STM32G431RB Nucleo-64:
 *   - blinks the on-board LED LD2 (PA5) at ~1 Hz
 *   - prints a heartbeat banner on the ST-LINK virtual COM port
 *     (USART2, PA2=TX / PA3=RX, 115200 8N1)
 *
 * Register-level init (no HAL) against CMSIS. Default reset clock is HSI 16 MHz;
 * SystemInit() (system_stm32g4xx.c) runs from the startup file before main().
 */
#include "stm32g4xx.h"

#include <stdint.h>

#define LED_PIN 5U /* PA5 = LD2 */

static volatile uint32_t s_ms;

void SysTick_Handler(void) { s_ms++; }

static void delay_ms(uint32_t ms)
{
    uint32_t start = s_ms;
    while ((s_ms - start) < ms) {
        __WFI();
    }
}

static void clock_ms_tick_init(void)
{
    /* 1 ms tick from the core clock (HSI 16 MHz after reset). */
    SysTick_Config(SystemCoreClock / 1000U);
}

static void led_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    /* PA5: general-purpose output (MODER = 01) */
    GPIOA->MODER &= ~GPIO_MODER_MODE5_Msk;
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODE5_Pos);
}

static void uart2_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

    /* PA2/PA3 -> alternate function (MODER = 10) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODE2_Pos) | (0x2U << GPIO_MODER_MODE3_Pos);
    /* AF7 = USART2 on PA2/PA3 */
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2_Msk | GPIO_AFRL_AFSEL3_Msk);
    GPIOA->AFR[0] |= (7U << GPIO_AFRL_AFSEL2_Pos) | (7U << GPIO_AFRL_AFSEL3_Pos);

    /* 115200 @ PCLK1 = 16 MHz (oversampling by 16): BRR = fck / baud */
    USART2->BRR = (SystemCoreClock + 57600U) / 115200U;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart2_write(const char *s)
{
    while (*s) {
        while (!(USART2->ISR & USART_ISR_TXE)) {
        }
        USART2->TDR = (uint8_t)*s++;
    }
}

int main(void)
{
    SystemCoreClockUpdate();
    clock_ms_tick_init();
    led_init();
    uart2_init();

    uart2_write("\r\nMicroPacControllerMan M1: toolchain bring-up OK\r\n");

    uint32_t beat = 0;
    char msg[48];
    for (;;) {
        GPIOA->ODR ^= (1U << LED_PIN); /* toggle LD2 */

        /* tiny int-to-string for the heartbeat counter (no printf dependency) */
        const char *p = "heartbeat ";
        char *m = msg;
        while (*p) {
            *m++ = *p++;
        }
        uint32_t v = beat++;
        char digits[10];
        int d = 0;
        do {
            digits[d++] = (char)('0' + (v % 10U));
            v /= 10U;
        } while (v && d < 10);
        while (d) {
            *m++ = digits[--d];
        }
        *m++ = '\r';
        *m++ = '\n';
        *m = '\0';
        uart2_write(msg);

        delay_ms(500); /* ~1 Hz blink (toggle every 500 ms) */
    }
}
