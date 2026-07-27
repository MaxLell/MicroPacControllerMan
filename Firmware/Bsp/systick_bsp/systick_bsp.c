#include "systick_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "main.h"

/* ==========================================================================
 * systick_bsp - private
 * ========================================================================= */

static systick_bsp_tick_callback_fn volatile g_tick_callback_fn = NULL;
static bool g_is_initialized = false;

/* ==========================================================================
 * systick_bsp - public
 * ========================================================================= */

void systick_bsp_init(void)
{
    ASSERT(false == g_is_initialized);

    g_tick_callback_fn = NULL;
    g_is_initialized = true;
}

uint32_t systick_bsp_get_tick(void)
{
    return HAL_GetTick();
}

void systick_bsp_register_tick_callback(systick_bsp_tick_callback_fn in_callback_fn)
{
    ASSERT(g_is_initialized);
    ASSERT(in_callback_fn != NULL);
    ASSERT(g_tick_callback_fn == NULL);

    g_tick_callback_fn = in_callback_fn;
}

/* Strong override of the HAL's __weak HAL_IncTick(), the documented extension
 * point for the 1 ms tick. Hooking in here keeps the CubeMX-generated
 * stm32g4xx_it.c untouched, so a re-generation cannot silently drop the hook. */
void HAL_IncTick(void)
{
    uwTick += uwTickFreq;

    if (g_tick_callback_fn != NULL)
    {
        g_tick_callback_fn();
    }
}
