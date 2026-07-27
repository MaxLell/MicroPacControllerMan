#include "ott_touchdot.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "display.h"
#include "gfx.h"
#include "sw_timer.h"
#include "touchpad.h"
#include "uart_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_touchdot - private
 * ========================================================================= */

#define OTT_TOUCHDOT_DOT_RADIUS (5)
#define OTT_TOUCHDOT_FRAME_PERIOD_MS (40U)

/* Safety cap, so the board returns to nominal mode even if the operator walks
 * away without confirming. */
#define OTT_TOUCHDOT_TIMEOUT_MS (120000U)

/* Pad-to-panel orientation, confirmed on hardware: the pad is mirrored on both
 * axes relative to the panel. Flip these if the dot ever tracks the wrong way —
 * SWAP exchanges the axes, the INVERT flags mirror one axis each. */
#define OTT_TOUCHDOT_SWAP_AXES (false)
#define OTT_TOUCHDOT_INVERT_X (true)
#define OTT_TOUCHDOT_INVERT_Y (true)

static sw_timer_t g_timeout_timer;
static sw_timer_t g_frame_timer;
static sw_timer_t g_vcom_timer;
static i2c_bsp_status_e g_frame_status;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_on_vcom_due(void)
{
    display_service_vcom();

    sw_timer_start(&g_vcom_timer, DISPLAY_VCOM_PERIOD_MS, prv_on_vcom_due);
}

/* Scale a raw pad coordinate onto a panel axis, applying the orientation flags. */
static int16_t prv_scale_to_panel(uint16_t in_raw, uint16_t in_raw_max, int16_t in_panel_size,
                                  bool in_is_inverted)
{
    const int16_t panel_max = (int16_t)(in_panel_size - 1);
    int16_t scaled = (int16_t)(((uint32_t)in_raw * (uint32_t)panel_max) / in_raw_max);

    if (in_is_inverted)
    {
        scaled = (int16_t)(panel_max - scaled);
    }

    return scaled;
}

static void prv_on_frame_due(void)
{
    touchpad_reading_t reading;
    int16_t dot_x;
    int16_t dot_y;

    g_frame_status = touchpad_read(&reading);

    if (g_frame_status != I2C_BSP_STATUS_OK)
    {
        return;
    }

    gfx_fill(DISPLAY_COLOR_WHITE);
    gfx_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_COLOR_BLACK);

    if (reading.is_touched)
    {
        if (OTT_TOUCHDOT_SWAP_AXES)
        {
            dot_x = prv_scale_to_panel(reading.y, TOUCHPAD_Y_MAX, DISPLAY_WIDTH,
                                       OTT_TOUCHDOT_INVERT_X);
            dot_y = prv_scale_to_panel(reading.x, TOUCHPAD_X_MAX, DISPLAY_HEIGHT,
                                       OTT_TOUCHDOT_INVERT_Y);
        }
        else
        {
            dot_x = prv_scale_to_panel(reading.x, TOUCHPAD_X_MAX, DISPLAY_WIDTH,
                                       OTT_TOUCHDOT_INVERT_X);
            dot_y = prv_scale_to_panel(reading.y, TOUCHPAD_Y_MAX, DISPLAY_HEIGHT,
                                       OTT_TOUCHDOT_INVERT_Y);
        }

        gfx_filled_circle(dot_x, dot_y, OTT_TOUCHDOT_DOT_RADIUS, DISPLAY_COLOR_BLACK);
        gfx_horizontal_line(0, dot_y, DISPLAY_WIDTH, DISPLAY_COLOR_BLACK);
        gfx_vertical_line(dot_x, 0, DISPLAY_HEIGHT, DISPLAY_COLOR_BLACK);
    }

    display_flush();

    sw_timer_start(&g_frame_timer, OTT_TOUCHDOT_FRAME_PERIOD_MS, prv_on_frame_due);
}

/* ==========================================================================
 * ott_touchdot - public
 * ========================================================================= */

bool ott_touchdot_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                        uint32_t* out_parameter_size)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)out_parameter;

    *out_parameter_size = 0U;

    return true;
}

bool ott_touchdot_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                      size_t in_reason_size)
{
    (void)in_parameter;
    (void)in_parameter_size;

    display_init();
    touchpad_init();

    if (touchpad_probe() != I2C_BSP_STATUS_OK)
    {
        (void)snprintf(out_reason, in_reason_size,
                       "touch controller not responding on I2C (check slot 2 / SDA-SCL map)");

        return false;
    }

    g_frame_status = I2C_BSP_STATUS_OK;

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_frame_timer);
    sw_timer_create(&g_vcom_timer);

    uart_bsp_write_string("Touch-dot test: the dot on the LCD follows your finger on the pad.\r\n");
    uart_bsp_write_string("Press the USER button (B1) to finish.\r\n");

    sw_timer_start(&g_timeout_timer, OTT_TOUCHDOT_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_frame_timer, OTT_TOUCHDOT_FRAME_PERIOD_MS, prv_on_frame_due);
    sw_timer_start(&g_vcom_timer, DISPLAY_VCOM_PERIOD_MS, prv_on_vcom_due);

    while (sw_timer_is_active(&g_timeout_timer) && (g_frame_status == I2C_BSP_STATUS_OK)
           && !user_button_take_press())
    {
        sw_timer_process();
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_frame_timer);
    sw_timer_stop(&g_vcom_timer);

    if (g_frame_status != I2C_BSP_STATUS_OK)
    {
        (void)snprintf(out_reason, in_reason_size, "I2C read failed during tracking (status %d)",
                       (int)g_frame_status);

        return false;
    }

    return true;
}
