#include "sw_timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "systick_bsp.h"

/* ==========================================================================
 * sw_timer - private
 * ========================================================================= */

static sw_timer_t* g_timer_table[SW_TIMER_MAX_TIMERS];
static bool g_is_initialized = false;

static bool prv_has_expired(const sw_timer_t* const in_timer, uint32_t in_now)
{
    return (uint32_t)(in_now - in_timer->start_tick) >= in_timer->timeout_ms;
}

static bool prv_is_registered_timer(const sw_timer_t* const in_timer)
{
    for (size_t index = 0U; index < SW_TIMER_MAX_TIMERS; ++index)
    {
        if (g_timer_table[index] == in_timer)
        {
            return true;
        }
    }

    return false;
}

/* ==========================================================================
 * sw_timer - public
 * ========================================================================= */

void sw_timer_init(void)
{
    ASSERT(false == g_is_initialized);

    for (size_t index = 0U; index < SW_TIMER_MAX_TIMERS; ++index)
    {
        g_timer_table[index] = NULL;
    }

    g_is_initialized = true;
}

void sw_timer_create(sw_timer_t* in_timer)
{
    ASSERT(g_is_initialized);
    ASSERT(in_timer != NULL);
    ASSERT(false == prv_is_registered_timer(in_timer));

    for (size_t index = 0U; index < SW_TIMER_MAX_TIMERS; ++index)
    {
        if (g_timer_table[index] == NULL)
        {
            in_timer->start_tick = 0U;
            in_timer->timeout_ms = 0U;
            in_timer->callback_fn = NULL;
            in_timer->is_active = false;

            g_timer_table[index] = in_timer;

            return;
        }
    }

    /* Out of slots — raise SW_TIMER_MAX_TIMERS. */
    ASSERT(false);
}

void sw_timer_start(sw_timer_t* in_timer, uint32_t in_timeout_ms,
                    sw_timer_callback_fn in_callback_fn)
{
    ASSERT(g_is_initialized);
    ASSERT(in_timer != NULL);
    ASSERT(in_callback_fn != NULL);
    ASSERT(prv_is_registered_timer(in_timer));

    in_timer->start_tick = systick_bsp_get_tick();
    in_timer->timeout_ms = in_timeout_ms;
    in_timer->callback_fn = in_callback_fn;
    in_timer->is_active = true;
}

void sw_timer_reset(sw_timer_t* in_timer)
{
    ASSERT(g_is_initialized);
    ASSERT(in_timer != NULL);
    ASSERT(prv_is_registered_timer(in_timer));
    ASSERT(in_timer->is_active);

    in_timer->start_tick = systick_bsp_get_tick();
}

void sw_timer_stop(sw_timer_t* in_timer)
{
    ASSERT(g_is_initialized);
    ASSERT(in_timer != NULL);
    ASSERT(prv_is_registered_timer(in_timer));

    in_timer->is_active = false;
}

bool sw_timer_is_active(const sw_timer_t* in_timer)
{
    ASSERT(g_is_initialized);
    ASSERT(in_timer != NULL);
    ASSERT(prv_is_registered_timer(in_timer));

    return in_timer->is_active;
}

void sw_timer_process(void)
{
    uint32_t now;

    ASSERT(g_is_initialized);

    now = systick_bsp_get_tick();

    for (size_t index = 0U; index < SW_TIMER_MAX_TIMERS; ++index)
    {
        sw_timer_t* const timer = g_timer_table[index];

        if ((timer != NULL) && timer->is_active && prv_has_expired(timer, now))
        {
            const sw_timer_callback_fn callback_fn = timer->callback_fn;

            /* Clear before the callback so it may re-arm the timer to make it
             * periodic. */
            timer->is_active = false;

            callback_fn();
        }
    }
}
