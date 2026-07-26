#include "uart.h"

#include "usart.h" /* hlpuart1 + HAL (from the CubeMX export) */

void uart_init(void)
{
    /* CubeMX's MX_LPUART1_UART_Init() already brought up PA2/PA3 + LPUART1. The
     * VCP console contract is fixed at 115200 8N1 (run_ott.py / console.py), so
     * pin the baud rate here regardless of the .ioc value and re-init. This is
     * the single source of truth for the console rate and survives a CubeMX
     * re-generation. */
    hlpuart1.Init.BaudRate = 115200;
    HAL_UART_Init(&hlpuart1);
}

int uart_putc(char c)
{
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)&c, 1U, HAL_MAX_DELAY);
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
    /* Non-blocking: the FIFO is disabled (CubeMX), so RXNE marks one ready byte.
     * Reading RDR clears the flag. */
    if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE)) {
        return (int)(hlpuart1.Instance->RDR & 0xFFU);
    }
    return -1;
}

void uart_flush(void)
{
    /* Block until the last byte has fully left the shift register. */
    while (!__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_TC)) {
    }
}
