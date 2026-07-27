#include "delay.h"

#include <stdint.h>

#include "systick_bsp.h"

/* ==========================================================================
 * delay - public
 * ========================================================================= */

void delay_ms(uint32_t in_delay_ms)
{
    const uint32_t start_tick = systick_bsp_get_tick();

    while ((uint32_t)(systick_bsp_get_tick() - start_tick) < in_delay_ms) {}
}
