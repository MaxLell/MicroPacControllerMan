#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/*
 * 1 kHz SysTick time base. Owns the millisecond counter and the SysTick_Handler
 * so that both the nominal super-loop and the OTT scenarios (which run before the
 * main loop) share one non-blocking delay/timing source.
 */

void     systick_init(void);   /* configure SysTick for a 1 ms tick */
uint32_t millis(void);         /* milliseconds since systick_init() */
void     delay_ms(uint32_t ms); /* busy-wait (non-blocking-safe) delay */

#endif /* SYSTICK_H */
