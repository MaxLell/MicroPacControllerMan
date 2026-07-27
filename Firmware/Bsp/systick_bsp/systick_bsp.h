/*
 * systick_bsp.h
 *
 * 1 kHz millisecond tick source, plus a hook for work that has to run on every
 * tick (input debouncing).
 */

#ifndef SYSTICK_BSP_H
#define SYSTICK_BSP_H

#include <stdint.h>

/* ==========================================================================
 * systick_bsp - public types
 * ========================================================================= */

typedef void (*systick_bsp_tick_callback_fn)(void);

/* ==========================================================================
 * systick_bsp - public API
 * ========================================================================= */

/*! \brief Initialize the tick source.
 *
 * The core SysTick already runs at 1 kHz when app_main() is entered (started by
 * HAL_Init() and re-based by SystemClock_Config()), so this only resets the
 * module state. Must be called exactly once.
 */
void systick_bsp_init(void);

/*! \brief Return the number of milliseconds elapsed since reset.
 *
 * Wraps around after ~49 days, so always compare differences, never absolutes.
 *
 * \return          Current tick in milliseconds
 */
uint32_t systick_bsp_get_tick(void);

/*! \brief Register the callback invoked from the 1 ms tick interrupt.
 *
 * The callback runs in interrupt context and must be short and non-blocking.
 * At most one callback can be registered.
 *
 * \param[in]       in_callback_fn: function to call every millisecond
 */
void systick_bsp_register_tick_callback(systick_bsp_tick_callback_fn in_callback_fn);

#endif /* SYSTICK_BSP_H */
