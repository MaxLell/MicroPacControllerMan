#include "uart_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "usart.h"

/* ==========================================================================
 * uart_bsp - private
 * ========================================================================= */

/* The console instance. LPUART1 (PA2/PA3) is the NUCLEO-G431RB ST-LINK virtual
 * COM port; the handle itself is brought up by the CubeMX MX_LPUART1_UART_Init().
 * Point this at another handle to move the console. */
#define UART_BSP_HANDLE (hlpuart1)

/* The console contract the host tools rely on (Test/run_ott.py, Test/console.py)
 * is 115200 8N1. Pinning the rate here rather than in the .ioc makes this the
 * single source of truth and survives a CubeMX re-generation. */
#define UART_BSP_BAUD_RATE (115200U)

#define UART_BSP_TRANSFER_SIZE_ONE_CHARACTER (1U)
#define UART_BSP_RECEIVED_DATA_MASK (0xFFU)

/* ==========================================================================
 * uart_bsp - public
 * ========================================================================= */

void uart_bsp_init(void)
{
    HAL_StatusTypeDef status;

    UART_BSP_HANDLE.Init.BaudRate = UART_BSP_BAUD_RATE;

    status = HAL_UART_Init(&UART_BSP_HANDLE);

    ASSERT(status == HAL_OK);
}

void uart_bsp_write_character(char in_character)
{
    (void)HAL_UART_Transmit(&UART_BSP_HANDLE, (const uint8_t*)&in_character,
                            UART_BSP_TRANSFER_SIZE_ONE_CHARACTER, HAL_MAX_DELAY);
}

void uart_bsp_write_string(const char* const in_string)
{
    ASSERT(in_string != NULL);

    for (size_t index = 0U; in_string[index] != '\0'; ++index)
    {
        uart_bsp_write_character(in_string[index]);
    }
}

bool uart_bsp_read_character(char* out_character)
{
    ASSERT(out_character != NULL);

    /* An overrun latches ORE, and while ORE is set the hardware stops raising
     * RXNE — so a single dropped character would wedge the console permanently.
     * Clear it and carry on: losing the overrun character is recoverable, losing
     * the console is not. */
    if (__HAL_UART_GET_FLAG(&UART_BSP_HANDLE, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_OREFLAG(&UART_BSP_HANDLE);
    }

    /* The RX FIFO is disabled, so RXNE marks exactly one ready character and
     * reading RDR clears the flag. The HAL has no non-blocking single-character
     * read, so this is the one place where a peripheral register is touched
     * directly — HAL_UART_Receive() would block or leave an error state behind. */
    if (!__HAL_UART_GET_FLAG(&UART_BSP_HANDLE, UART_FLAG_RXNE))
    {
        return false;
    }

    *out_character = (char)(UART_BSP_HANDLE.Instance->RDR & UART_BSP_RECEIVED_DATA_MASK);

    return true;
}

void uart_bsp_flush(void)
{
    while (!__HAL_UART_GET_FLAG(&UART_BSP_HANDLE, UART_FLAG_TC)) {}
}
