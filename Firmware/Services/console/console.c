#include "console.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "uart_bsp.h"

/* ==========================================================================
 * console - private
 * ========================================================================= */

/* Received characters, between the tick that samples them and the loop that reads them.
 *
 * Hand-rolled rather than `Services/circular_buffer`, and that is the point of the
 * comment: this one is filled from an interrupt and drained from the main loop, and that
 * buffer keeps a single `count` which both sides would have to read-modify-write. A tick
 * landing between the reader's load and store of it loses the writer's increment. Here the
 * producer only ever writes `head` and the consumer only ever writes `tail`, so neither
 * touches the other's variable and no interrupts have to be disabled.
 *
 * Sixty-four is far more than a command line; the point is only to cover a burst that
 * arrives between two ticks. */
#define CONSOLE_RECEIVE_CAPACITY (64U)

static volatile char g_receive_storage[CONSOLE_RECEIVE_CAPACITY];
static volatile uint8_t g_receive_head; /* written by console_poll_receive only  */
static volatile uint8_t g_receive_tail; /* written by console_read_character only */

static uint8_t prv_next_index(uint8_t in_index)
{
    return (uint8_t)((in_index + 1U) % CONSOLE_RECEIVE_CAPACITY);
}

/* ==========================================================================
 * console - public
 * ========================================================================= */

void console_init(void)
{
    g_receive_head = 0U;
    g_receive_tail = 0U;

    uart_bsp_init();
}

void console_poll_receive(void)
{
    char character;

    while (uart_bsp_read_character(&character))
    {
        const uint8_t next = prv_next_index(g_receive_head);

        if (next == g_receive_tail)
        {
            /* Full. Dropping is what the hardware would have done anyway, and a console
             * that wedges would be worse than one that loses a keystroke. */
            return;
        }

        g_receive_storage[g_receive_head] = character;
        g_receive_head = next;
    }
}

void console_write_character(char in_character)
{
    uart_bsp_write_character(in_character);
}

bool console_read_character(char* out_character)
{
    ASSERT(out_character != NULL);

    if (g_receive_tail == g_receive_head)
    {
        return false;
    }

    *out_character = g_receive_storage[g_receive_tail];
    g_receive_tail = prv_next_index(g_receive_tail);

    return true;
}

void console_flush(void)
{
    uart_bsp_flush();
}
