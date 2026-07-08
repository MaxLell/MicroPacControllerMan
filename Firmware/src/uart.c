#include "uart.h"

#include "stm32g4xx.h"

void uart_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;

    /* PA2/PA3 -> alternate function (MODER = 10) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (0x2U << GPIO_MODER_MODE2_Pos) | (0x2U << GPIO_MODER_MODE3_Pos);
    /* AF12 = LPUART1 on PA2/PA3 (the NUCLEO-G431RB ST-LINK VCP) */
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2_Msk | GPIO_AFRL_AFSEL3_Msk);
    GPIOA->AFR[0] |= (12U << GPIO_AFRL_AFSEL2_Pos) | (12U << GPIO_AFRL_AFSEL3_Pos);

    /* LPUART baud: BRR = (256 * f_ck) / baud, f_ck = PCLK1 (HSI 16 MHz) */
    LPUART1->BRR = (uint32_t)(((uint64_t)256U * SystemCoreClock) / 115200U);
    LPUART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

int uart_putc(char c)
{
    while (!(LPUART1->ISR & USART_ISR_TXE)) {
    }
    LPUART1->TDR = (uint8_t)c;
    return 0;
}

void uart_write(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

int uart_getc(void)
{
    if (LPUART1->ISR & USART_ISR_RXNE) {
        return (int)(LPUART1->RDR & 0xFFU);
    }
    return -1;
}
