/*
 * uart_bsp.h
 *
 * Blocking serial console transport. The UART instance and the line settings are
 * a single #define in uart_bsp.c, so the module moves to another project by
 * changing that one line.
 */

#ifndef UART_BSP_H
#define UART_BSP_H

#include <stdbool.h>

/* ==========================================================================
 * uart_bsp - public API
 * ========================================================================= */

/*! \brief Bring the console UART up at the fixed console line rate. */
void uart_bsp_init(void);

/*! \brief Write one character, blocking until the UART has accepted it.
 *
 * \param[in]       in_character: character to send
 */
void uart_bsp_write_character(char in_character);

/*! \brief Write a NUL-terminated string, blocking until the last character has
 *         been accepted by the UART.
 *
 * \param[in]       in_string: string to send, must not be `NULL`
 */
void uart_bsp_write_string(const char* const in_string);

/*! \brief Read one received character without blocking.
 *
 * \param[out]      out_character: receives the character when one was available
 * \return          `true` when a character was read, `false` when none was ready
 */
bool uart_bsp_read_character(char* out_character);

/*! \brief Block until the last character has fully left the shift register.
 *
 * Needed before a deliberate reset, so a final message is not truncated.
 */
void uart_bsp_flush(void);

#endif /* UART_BSP_H */
