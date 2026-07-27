#include "ott_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "gfx.h"
#include "sw_timer.h"
#include "uart_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_display - private
 * ========================================================================= */

#define OTT_DISPLAY_SCENE_HOLD_MS (1500U)

/* Safety cap, so the board returns to nominal mode even if the operator walks
 * away without confirming. */
#define OTT_DISPLAY_TIMEOUT_MS (120000U)

/* Geometry of the scenes, in pixels. */
#define OTT_DISPLAY_RAY_SPACING (8)
#define OTT_DISPLAY_RING_SPACING (6)
#define OTT_DISPLAY_BAND_SPACING (12)
#define OTT_DISPLAY_CIRCLE_SPACING (8)
#define OTT_DISPLAY_CIRCLE_FIRST_RADIUS (6)
#define OTT_DISPLAY_DOT_GRID_SPACING (32)
#define OTT_DISPLAY_DOT_GRID_ORIGIN (16)
#define OTT_DISPLAY_DOT_RADIUS (12)
#define OTT_DISPLAY_WEDGE_SPACING (8)

/* Composite scene, laid out by hand. */
#define OTT_DISPLAY_BOX_ORIGIN (8)
#define OTT_DISPLAY_BOX_SIZE (40)
#define OTT_DISPLAY_EYE_CENTER_X (96)
#define OTT_DISPLAY_EYE_CENTER_Y (28)
#define OTT_DISPLAY_EYE_OUTER_RADIUS (20)
#define OTT_DISPLAY_EYE_INNER_RADIUS (10)
#define OTT_DISPLAY_DIAGONAL_START_Y (120)
#define OTT_DISPLAY_DIAGONAL_END_X (120)
#define OTT_DISPLAY_DIAGONAL_END_Y (60)
#define OTT_DISPLAY_WEDGE_LEFT_X (20)
#define OTT_DISPLAY_WEDGE_BASE_Y (118)
#define OTT_DISPLAY_WEDGE_APEX_X (64)
#define OTT_DISPLAY_WEDGE_APEX_Y (70)
#define OTT_DISPLAY_WEDGE_RIGHT_X (108)

typedef void (*ott_display_scene_fn)(void);

static sw_timer_t g_timeout_timer;
static sw_timer_t g_hold_timer;
static sw_timer_t g_vcom_timer;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

static void prv_on_hold_elapsed(void)
{
    /* Nothing to do: prv_hold() watches sw_timer_is_active(). */
}

static void prv_on_vcom_due(void)
{
    display_service_vcom();

    sw_timer_start(&g_vcom_timer, DISPLAY_VCOM_PERIOD_MS, prv_on_vcom_due);
}

/* Hold the current frame, keeping the panel's COM inversion alive. Returns `true`
 * when the operator pressed the button, to finish early. */
static bool prv_hold(uint32_t in_duration_ms)
{
    sw_timer_start(&g_hold_timer, in_duration_ms, prv_on_hold_elapsed);

    while (sw_timer_is_active(&g_hold_timer))
    {
        sw_timer_process();

        if (user_button_take_press())
        {
            sw_timer_stop(&g_hold_timer);

            return true;
        }
    }

    return false;
}

static void prv_scene_rays(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t x = 0; x < DISPLAY_WIDTH; x = (int16_t)(x + OTT_DISPLAY_RAY_SPACING))
    {
        gfx_line(0, 0, x, DISPLAY_HEIGHT - 1, DISPLAY_COLOR_BLACK);
    }

    for (int16_t y = 0; y < DISPLAY_HEIGHT; y = (int16_t)(y + OTT_DISPLAY_RAY_SPACING))
    {
        gfx_line(DISPLAY_WIDTH - 1, 0, 0, y, DISPLAY_COLOR_BLACK);
    }
}

static void prv_scene_nested_rectangles(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t inset = 0; inset < (DISPLAY_WIDTH / 2);
         inset = (int16_t)(inset + OTT_DISPLAY_RING_SPACING))
    {
        gfx_rectangle(inset, inset, (int16_t)(DISPLAY_WIDTH - (2 * inset)),
                      (int16_t)(DISPLAY_HEIGHT - (2 * inset)), DISPLAY_COLOR_BLACK);
    }
}

static void prv_scene_alternating_bands(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t inset = 0; inset < (DISPLAY_WIDTH / 2);
         inset = (int16_t)(inset + OTT_DISPLAY_BAND_SPACING))
    {
        const display_color_e color = (((inset / OTT_DISPLAY_BAND_SPACING) % 2) != 0)
                                          ? DISPLAY_COLOR_WHITE
                                          : DISPLAY_COLOR_BLACK;

        gfx_filled_rectangle(inset, inset, (int16_t)(DISPLAY_WIDTH - (2 * inset)),
                             (int16_t)(DISPLAY_HEIGHT - (2 * inset)), color);
    }
}

static void prv_scene_concentric_circles(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t radius = OTT_DISPLAY_CIRCLE_FIRST_RADIUS; radius < DISPLAY_WIDTH;
         radius = (int16_t)(radius + OTT_DISPLAY_CIRCLE_SPACING))
    {
        gfx_circle(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, radius, DISPLAY_COLOR_BLACK);
    }
}

static void prv_scene_dot_grid(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t y = OTT_DISPLAY_DOT_GRID_ORIGIN; y < DISPLAY_HEIGHT;
         y = (int16_t)(y + OTT_DISPLAY_DOT_GRID_SPACING))
    {
        for (int16_t x = OTT_DISPLAY_DOT_GRID_ORIGIN; x < DISPLAY_WIDTH;
             x = (int16_t)(x + OTT_DISPLAY_DOT_GRID_SPACING))
        {
            gfx_filled_circle(x, y, OTT_DISPLAY_DOT_RADIUS, DISPLAY_COLOR_BLACK);
        }
    }
}

static void prv_scene_nested_triangles(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    for (int16_t inset = 0; inset < (DISPLAY_HEIGHT / 2);
         inset = (int16_t)(inset + OTT_DISPLAY_WEDGE_SPACING))
    {
        gfx_triangle(DISPLAY_WIDTH / 2, inset, inset, (int16_t)(DISPLAY_HEIGHT - 1 - inset),
                     (int16_t)(DISPLAY_WIDTH - 1 - inset), (int16_t)(DISPLAY_HEIGHT - 1 - inset),
                     DISPLAY_COLOR_BLACK);
    }
}

static void prv_scene_composite(void)
{
    gfx_fill(DISPLAY_COLOR_WHITE);

    gfx_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_COLOR_BLACK);
    gfx_filled_rectangle(OTT_DISPLAY_BOX_ORIGIN, OTT_DISPLAY_BOX_ORIGIN, OTT_DISPLAY_BOX_SIZE,
                         OTT_DISPLAY_BOX_SIZE, DISPLAY_COLOR_BLACK);
    gfx_circle(OTT_DISPLAY_EYE_CENTER_X, OTT_DISPLAY_EYE_CENTER_Y, OTT_DISPLAY_EYE_OUTER_RADIUS,
               DISPLAY_COLOR_BLACK);
    gfx_filled_circle(OTT_DISPLAY_EYE_CENTER_X, OTT_DISPLAY_EYE_CENTER_Y,
                      OTT_DISPLAY_EYE_INNER_RADIUS, DISPLAY_COLOR_BLACK);
    gfx_line(OTT_DISPLAY_BOX_ORIGIN, OTT_DISPLAY_DIAGONAL_START_Y, OTT_DISPLAY_DIAGONAL_END_X,
             OTT_DISPLAY_DIAGONAL_END_Y, DISPLAY_COLOR_BLACK);
    gfx_filled_triangle(OTT_DISPLAY_WEDGE_LEFT_X, OTT_DISPLAY_WEDGE_BASE_Y,
                        OTT_DISPLAY_WEDGE_APEX_X, OTT_DISPLAY_WEDGE_APEX_Y,
                        OTT_DISPLAY_WEDGE_RIGHT_X, OTT_DISPLAY_WEDGE_BASE_Y, DISPLAY_COLOR_BLACK);
}

static const ott_display_scene_fn k_scenes[] = {
    prv_scene_rays,          prv_scene_nested_rectangles, prv_scene_alternating_bands,
    prv_scene_concentric_circles, prv_scene_dot_grid,     prv_scene_nested_triangles,
    prv_scene_composite,
};

/* ==========================================================================
 * ott_display - public
 * ========================================================================= */

bool ott_display_setup(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                       uint32_t* out_parameter_size)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)out_parameter;

    *out_parameter_size = 0U;

    return true;
}

bool ott_display_run(const uint8_t* in_parameter, uint32_t in_parameter_size, char* out_reason,
                     size_t in_reason_size)
{
    const size_t scene_count = sizeof(k_scenes) / sizeof(k_scenes[0]);
    bool is_confirmed = false;

    (void)in_parameter;
    (void)in_parameter_size;
    (void)out_reason;
    (void)in_reason_size;

    display_init();

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_hold_timer);
    sw_timer_create(&g_vcom_timer);

    uart_bsp_write_string("Display test: geometric patterns on the LCD.\r\n");
    uart_bsp_write_string("Watch the panel cycle lines/rects/circles/triangles, then hold.\r\n");
    uart_bsp_write_string("Press the USER button (B1) to finish.\r\n");

    sw_timer_start(&g_timeout_timer, OTT_DISPLAY_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_vcom_timer, DISPLAY_VCOM_PERIOD_MS, prv_on_vcom_due);

    for (size_t index = 0U; (index < scene_count) && !is_confirmed; ++index)
    {
        k_scenes[index]();
        display_flush();

        is_confirmed = prv_hold(OTT_DISPLAY_SCENE_HOLD_MS);
    }

    /* Hold the composite until the operator confirms, or the safety cap hits. */
    if (!is_confirmed)
    {
        prv_scene_composite();
        display_flush();

        while (sw_timer_is_active(&g_timeout_timer) && !prv_hold(OTT_DISPLAY_SCENE_HOLD_MS)) {}
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_vcom_timer);

    return true;
}
