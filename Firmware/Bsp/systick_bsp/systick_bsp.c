#include "systick.h"

#include "main.h" /* HAL_GetTick / HAL_Delay (from the CubeMX export) */

/*
 * Thin shim over the HAL time base. HAL owns the 1 kHz SysTick: it is started by
 * HAL_Init() and re-based by SystemClock_Config(), and its SysTick_Handler (which
 * calls HAL_IncTick()) lives in the generated stm32g4xx_it.c — so this module
 * must NOT define its own handler or counter.
 */

void systick_init(void)
{
    /* SysTick is configured by HAL_Init() / SystemClock_Config(); nothing to do. */
}

uint32_t millis(void)
{
    return HAL_GetTick();
}

void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}
