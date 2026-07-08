#ifndef UART_H
#define UART_H

/* Serial console on LPUART1 (PA2/PA3, AF12) = NUCLEO-G431RB ST-LINK VCP, 115200 8N1. */
void uart_init(void);
void uart_write(const char *s);
int  uart_getc(void); /* next received byte, or -1 if none available (non-blocking) */

#endif /* UART_H */
