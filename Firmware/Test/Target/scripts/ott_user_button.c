#include "ott_user_button.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "sw_timer.h"
#include "user_button.h"

/* ==========================================================================
 * ott_user_button - private
 * ========================================================================= */

#define OTT_USER_BUTTON_REQUIRED_PRESSES    (3U)

/* Diagnostic cap, so the board returns to nominal mode even if nothing happens. */
#define OTT_USER_BUTTON_TIMEOUT_MS          (30000U)
#define OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS (1000U)

#define OTT_USER_BUTTON_MS_PER_SECOND       (1000U)

static sw_timer_t g_timeout_timer;
static sw_timer_t g_heartbeat_timer;
static uint32_t g_press_count;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_on_heartbeat(void)
{
    cli_print("BTN alive pressed=%u presses=%lu", user_button_is_pressed() ? 1U : 0U, (unsigned long)g_press_count);

    sw_timer_start(&g_heartbeat_timer, OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);
}

/* ==========================================================================
 * ott_user_button - public
 * ========================================================================= */

bool ott_user_button_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_passed;

    (void)in_parameter;

    g_press_count = 0U;

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_heartbeat_timer);

    cli_print("User-button test: press the USER button (B1) %u times.", OTT_USER_BUTTON_REQUIRED_PRESSES);
    cli_print("Live state below (pressed=1 while held); times out after %u s.",
              OTT_USER_BUTTON_TIMEOUT_MS / OTT_USER_BUTTON_MS_PER_SECOND);

    sw_timer_start(&g_timeout_timer, OTT_USER_BUTTON_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_heartbeat_timer, OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);

    while (sw_timer_is_active(&g_timeout_timer) && (g_press_count < OTT_USER_BUTTON_REQUIRED_PRESSES))
    {
        sw_timer_process();

        if (user_button_take_press())
        {
            ++g_press_count;

            cli_print("BTN press #%lu", (unsigned long)g_press_count);
        }
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_heartbeat_timer);

    has_passed = (g_press_count >= OTT_USER_BUTTON_REQUIRED_PRESSES);

    if (!has_passed)
    {
        /* B1 is active HIGH, so a press that never registers means the pin stayed low. */
        (void)snprintf(out_reason, in_reason_size, "only %lu/%u presses seen (PC13 stuck low?)",
                       (unsigned long)g_press_count, OTT_USER_BUTTON_REQUIRED_PRESSES);
    }

    return has_passed;
}
