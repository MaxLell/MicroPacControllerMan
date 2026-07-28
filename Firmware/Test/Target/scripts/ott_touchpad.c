#include "ott_touchpad.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sw_timer.h"
#include "touchpad.h"
#include "uart_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_touchpad - private
 * ========================================================================= */

/* Safety cap, so the board returns to nominal mode even if the operator walks
 * away without confirming. */
#define OTT_TOUCHPAD_TIMEOUT_MS       (120000U)
#define OTT_TOUCHPAD_SAMPLE_PERIOD_MS (120U)

#define OTT_TOUCHPAD_LINE_MAX_SIZE    (64U)

static sw_timer_t g_timeout_timer;
static sw_timer_t g_sample_timer;
static i2c_bsp_status_e g_sample_status;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_on_sample_due(void)
{
    touchpad_reading_t reading;
    char line[OTT_TOUCHPAD_LINE_MAX_SIZE];

    g_sample_status = touchpad_read(&reading);

    if (g_sample_status != I2C_BSP_STATUS_OK)
    {
        return;
    }

    (void)snprintf(line, sizeof(line), "TP touch=%u x=%3u y=%3u\r\n", reading.is_touched ? 1U : 0U, (unsigned)reading.x,
                   (unsigned)reading.y);
    uart_bsp_write_string(line);

    sw_timer_start(&g_sample_timer, OTT_TOUCHPAD_SAMPLE_PERIOD_MS, prv_on_sample_due);
}

/* ==========================================================================
 * ott_touchpad - public
 * ========================================================================= */

bool ott_touchpad_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                        uint32_t* out_parameter_size)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)out_parameter;

    *out_parameter_size = 0U;

    return true;
}

bool ott_touchpad_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason, size_t in_reason_size)
{
    (void)in_parameter;
    (void)in_parameter_size;

    touchpad_init();

    if (touchpad_probe() != I2C_BSP_STATUS_OK)
    {
        (void)snprintf(out_reason, in_reason_size,
                       "touch controller not responding on I2C (check slot 2 / SDA-SCL map)");

        return false;
    }

    g_sample_status = I2C_BSP_STATUS_OK;

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_sample_timer);

    uart_bsp_write_string("Touchpad live read. Move your finger on the pad; the x/y/touch\r\n");
    uart_bsp_write_string("values below should follow it. Press the USER button (B1) to finish.\r\n");
    uart_bsp_write_string("(The controller reports position + touch-present, no pressure.)\r\n");

    sw_timer_start(&g_timeout_timer, OTT_TOUCHPAD_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_sample_timer, OTT_TOUCHPAD_SAMPLE_PERIOD_MS, prv_on_sample_due);

    while (sw_timer_is_active(&g_timeout_timer) && (g_sample_status == I2C_BSP_STATUS_OK) && !user_button_take_press())
    {
        sw_timer_process();
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_sample_timer);

    if (g_sample_status != I2C_BSP_STATUS_OK)
    {
        (void)snprintf(out_reason, in_reason_size, "I2C read failed during live sampling (status %d)",
                       (int)g_sample_status);

        return false;
    }

    return true;
}
