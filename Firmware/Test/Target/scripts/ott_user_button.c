#include "ott_user_button.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "sw_timer.h"
#include "uart_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_user_button - private
 * ========================================================================= */

#define OTT_USER_BUTTON_REQUIRED_PRESSES (3U)

/* Diagnostic cap, so the board returns to nominal mode even if nothing happens. */
#define OTT_USER_BUTTON_TIMEOUT_MS (30000U)
#define OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS (1000U)

#define OTT_USER_BUTTON_LINE_MAX_SIZE (72U)

static sw_timer_t g_timeout_timer;
static sw_timer_t g_heartbeat_timer;
static uint32_t g_press_count;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_on_heartbeat(void)
{
    char line[OTT_USER_BUTTON_LINE_MAX_SIZE];

    (void)snprintf(line, sizeof(line), "BTN alive pressed=%u presses=%lu\r\n",
                   user_button_is_pressed() ? 1U : 0U, (unsigned long)g_press_count);
    uart_bsp_write_string(line);

    sw_timer_start(&g_heartbeat_timer, OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);
}

/* ==========================================================================
 * ott_user_button - public
 * ========================================================================= */

bool ott_user_button_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                           uint32_t* out_parameter_size)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)out_parameter;

    *out_parameter_size = 0U;

    return true;
}

bool ott_user_button_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                         size_t in_reason_size)
{
    char line[OTT_USER_BUTTON_LINE_MAX_SIZE];

    (void)in_parameter;
    (void)in_parameter_size;

    g_press_count = 0U;

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_heartbeat_timer);

    uart_bsp_write_string("User-button test: press the USER button (B1) three times.\r\n");
    uart_bsp_write_string("Live state below (pressed=1 while held); times out after 30 s.\r\n");

    sw_timer_start(&g_timeout_timer, OTT_USER_BUTTON_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_heartbeat_timer, OTT_USER_BUTTON_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);

    while (sw_timer_is_active(&g_timeout_timer)
           && (g_press_count < OTT_USER_BUTTON_REQUIRED_PRESSES))
    {
        sw_timer_process();

        if (user_button_take_press())
        {
            ++g_press_count;

            (void)snprintf(line, sizeof(line), "BTN press #%lu\r\n", (unsigned long)g_press_count);
            uart_bsp_write_string(line);
        }
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_heartbeat_timer);

    if (g_press_count >= OTT_USER_BUTTON_REQUIRED_PRESSES)
    {
        return true;
    }

    (void)snprintf(out_reason, in_reason_size, "only %lu/%u presses seen (PC13 stuck high?)",
                   (unsigned long)g_press_count, OTT_USER_BUTTON_REQUIRED_PRESSES);

    return false;
}
