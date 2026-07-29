#include "console.h"

#include <stdbool.h>
#include <stddef.h>

#include "custom_assert.h"
#include "uart_bsp.h"

/* ==========================================================================
 * console - public
 * ========================================================================= */

void console_init(void)
{
    uart_bsp_init();
}

void console_write_character(char in_character)
{
    uart_bsp_write_character(in_character);
}

bool console_read_character(char* out_character)
{
    ASSERT(out_character != NULL);

    return uart_bsp_read_character(out_character);
}

void console_flush(void)
{
    uart_bsp_flush();
}
