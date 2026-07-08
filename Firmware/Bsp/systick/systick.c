#include "systick.h"

#include "stm32g4xx.h"

static volatile uint32_t g_ms;

void SysTick_Handler(void) { g_ms++; }

void systick_init(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U); /* 1 ms tick */
}

uint32_t millis(void) { return g_ms; }

void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms;
    while ((g_ms - start) < ms) {
    }
}
