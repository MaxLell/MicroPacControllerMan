/*
 * Host implementation of the tick source, replacing systick_bsp.c in a host build.
 *
 * The header is shared with the target — this is the first of the platform ports
 * that [03 §3.8] calls for, and the pattern the display, input and NVM seams will
 * follow (RF-003).
 */
#include "systick_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "custom_assert.h"

/* ==========================================================================
 * systick_bsp - private
 * ========================================================================= */

#define SYSTICK_BSP_MILLISECONDS_PER_SECOND     (1000U)
#define SYSTICK_BSP_NANOSECONDS_PER_MILLISECOND (1000000U)

static systick_bsp_tick_callback_fn g_tick_callback_fn = NULL;
static bool g_is_initialized = false;
static uint64_t g_start_milliseconds = 0U;

static uint64_t prv_monotonic_milliseconds(void)
{
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);

    return ((uint64_t)now.tv_sec * SYSTICK_BSP_MILLISECONDS_PER_SECOND)
           + ((uint64_t)now.tv_nsec / SYSTICK_BSP_NANOSECONDS_PER_MILLISECOND);
}

/* ==========================================================================
 * systick_bsp - public
 * ========================================================================= */

void systick_bsp_init(void)
{
    ASSERT(false == g_is_initialized);

    /* Zero the origin so the host tick starts where the target's does, which keeps
     * timeout arithmetic identical across both builds. */
    g_start_milliseconds = prv_monotonic_milliseconds();
    g_tick_callback_fn = NULL;
    g_is_initialized = true;
}

uint32_t systick_bsp_get_tick(void)
{
    return (uint32_t)(prv_monotonic_milliseconds() - g_start_milliseconds);
}

void systick_bsp_register_tick_callback(systick_bsp_tick_callback_fn in_callback_fn)
{
    ASSERT(g_is_initialized);
    ASSERT(in_callback_fn != NULL);
    ASSERT(g_tick_callback_fn == NULL);

    /* Accepted and remembered, but never invoked: a host process has no 1 kHz
     * interrupt. The only current user is GPIO input debouncing, which does not
     * exist on the host — host input arrives as window events, already debounced.
     * Registering is therefore harmless and keeps the start-up sequence identical
     * across both builds. */
    g_tick_callback_fn = in_callback_fn;
}
