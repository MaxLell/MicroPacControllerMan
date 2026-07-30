#include "ott_display_test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "delay.h"
#include "display.h"
#include "framebuffer.h"
#include "gfx.h"
#include "st7789.h"
#include "systick_bsp.h"
#include "sw_timer.h"
#include "user_button.h"

/* ==========================================================================
 * ott_display_test - private
 * ========================================================================= */

#define OTT_DISPLAY_TEST_TIMEOUT_MS (120000U)
#define OTT_DISPLAY_TEST_MS_PER_SECOND (1000U)

#define OTT_DISPLAY_TEST_HOLD_MS (900U)

/* A marker small enough to leave the rest of the screen readable, big enough to see. */
#define OTT_DISPLAY_TEST_CORNER_SIZE (40U)
#define OTT_DISPLAY_TEST_BORDER_SIZE (4U)

#define OTT_DISPLAY_TEST_BAR_COUNT (8U)

/* Enough presents to average out the tick's 1 ms granularity. */
#define OTT_DISPLAY_TEST_FRAME_COUNT (5U)
#define OTT_DISPLAY_TEST_MS_PER_SECOND_F (1000.0)

/* 153,600 bytes. Static, because it does not fit on a stack. */
static framebuffer_t g_framebuffer;

static sw_timer_t g_timeout_timer;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

/* Each fill is announced before it is drawn, so a panel that stops responding can be
 * pinned to the step that did it. */
static void prv_show_screen(const char* const in_description, uint16_t in_colour)
{
    cli_print("  %s", in_description);
    st7789_fill_screen(in_colour);
    delay_ms(OTT_DISPLAY_TEST_HOLD_MS);
}

/* Vertical bars, so a wrong pixel format or byte order shows up as wrong or muddled
 * colours rather than as a plausible picture. */
static void prv_show_colour_bars(void)
{
    static const uint16_t bars[OTT_DISPLAY_TEST_BAR_COUNT] = {
        ST7789_RGB(0U, 0U, 0U),     ST7789_RGB(0U, 0U, 255U),   ST7789_RGB(255U, 0U, 0U),
        ST7789_RGB(255U, 0U, 255U), ST7789_RGB(0U, 255U, 0U),   ST7789_RGB(0U, 255U, 255U),
        ST7789_RGB(255U, 255U, 0U), ST7789_RGB(255U, 255U, 255U),
    };
    const uint16_t bar_width = ST7789_WIDTH / OTT_DISPLAY_TEST_BAR_COUNT;

    cli_print("  colour bars, left to right: black blue red magenta green cyan yellow white");

    for (uint16_t index = 0U; index < OTT_DISPLAY_TEST_BAR_COUNT; ++index)
    {
        st7789_fill_rectangle((uint16_t)(index * bar_width), 0U, bar_width, ST7789_HEIGHT,
                              bars[index]);
    }

    delay_ms(OTT_DISPLAY_TEST_HOLD_MS);
}

/* One white corner and a border: names the origin and proves the full 240x320 extent
 * is addressable, which a full-screen fill alone would not show. */
static void prv_show_geometry(void)
{
    const uint16_t white = ST7789_RGB(255U, 255U, 255U);
    const uint16_t red = ST7789_RGB(255U, 0U, 0U);

    cli_print("  geometry: red border around the full 240x320, white square at the origin");

    st7789_fill_screen(ST7789_RGB(0U, 0U, 0U));

    st7789_fill_rectangle(0U, 0U, ST7789_WIDTH, OTT_DISPLAY_TEST_BORDER_SIZE, red);
    st7789_fill_rectangle(0U, ST7789_HEIGHT - OTT_DISPLAY_TEST_BORDER_SIZE, ST7789_WIDTH,
                          OTT_DISPLAY_TEST_BORDER_SIZE, red);
    st7789_fill_rectangle(0U, 0U, OTT_DISPLAY_TEST_BORDER_SIZE, ST7789_HEIGHT, red);
    st7789_fill_rectangle(ST7789_WIDTH - OTT_DISPLAY_TEST_BORDER_SIZE, 0U, OTT_DISPLAY_TEST_BORDER_SIZE,
                          ST7789_HEIGHT, red);

    st7789_fill_rectangle(OTT_DISPLAY_TEST_BORDER_SIZE, OTT_DISPLAY_TEST_BORDER_SIZE,
                          OTT_DISPLAY_TEST_CORNER_SIZE, OTT_DISPLAY_TEST_CORNER_SIZE, white);
}

/* Draws through the real path — gfx into a frame buffer, then the display port — and
 * times it, because the frame budget is the open question of M2 and an estimate is not
 * a measurement. */
static void prv_measure_frame_rate(void)
{
    uint32_t start_tick;
    uint32_t elapsed_ms;
    double milliseconds_per_frame;

    cli_print("  timing a full-frame present through framebuffer -> gfx -> display");

    framebuffer_fill(&g_framebuffer, FRAMEBUFFER_COLOR_BLACK);
    gfx_filled_circle(&g_framebuffer, FRAMEBUFFER_WIDTH / 2, FRAMEBUFFER_HEIGHT / 2,
                      FRAMEBUFFER_WIDTH / 4, FRAMEBUFFER_COLOR_YELLOW);
    gfx_rectangle(&g_framebuffer, 0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT,
                  FRAMEBUFFER_COLOR_BLUE);

    start_tick = systick_bsp_get_tick();

    for (uint32_t frame = 0U; frame < OTT_DISPLAY_TEST_FRAME_COUNT; ++frame)
    {
        display_present(&g_framebuffer);
    }

    elapsed_ms = systick_bsp_get_tick() - start_tick;
    milliseconds_per_frame = (double)elapsed_ms / OTT_DISPLAY_TEST_FRAME_COUNT;

    cli_print("  %lu full frames in %lu ms -> %d ms/frame, %d fps",
              (unsigned long)OTT_DISPLAY_TEST_FRAME_COUNT, (unsigned long)elapsed_ms,
              (int)milliseconds_per_frame,
              (int)(OTT_DISPLAY_TEST_MS_PER_SECOND_F / milliseconds_per_frame));
}

/* ==========================================================================
 * ott_display_test - public
 * ========================================================================= */

bool ott_display_test_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    uint8_t id[ST7789_ID_LENGTH] = {0U};
    bool has_confirmed = false;

    (void)in_parameter;

    cli_print("Display test: bringing the panel up.");

    display_init();
    st7789_read_id(id);

    cli_print("  controller ID: %02X %02X %02X (%s)", (unsigned)id[0], (unsigned)id[1],
              (unsigned)id[2], st7789_is_present() ? "ST7789V" : "UNEXPECTED");

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "controller ID %02X %02X %02X is not an ST7789V",
                       (unsigned)id[0], (unsigned)id[1], (unsigned)id[2]);

        return false;
    }

    prv_show_screen("full red", ST7789_RGB(255U, 0U, 0U));
    prv_show_screen("full green", ST7789_RGB(0U, 255U, 0U));
    prv_show_screen("full blue", ST7789_RGB(0U, 0U, 255U));
    prv_show_colour_bars();
    prv_show_geometry();
    delay_ms(OTT_DISPLAY_TEST_HOLD_MS);

    prv_measure_frame_rate();

    cli_print("Press B1 if you saw red, green, blue, the bars, the border, then a yellow disc.");
    cli_print("Times out after %u s.", OTT_DISPLAY_TEST_TIMEOUT_MS / OTT_DISPLAY_TEST_MS_PER_SECOND);

    sw_timer_create(&g_timeout_timer);
    sw_timer_start(&g_timeout_timer, OTT_DISPLAY_TEST_TIMEOUT_MS, prv_on_timeout);

    while (sw_timer_is_active(&g_timeout_timer) && !has_confirmed)
    {
        sw_timer_process();

        has_confirmed = user_button_take_press();
    }

    sw_timer_stop(&g_timeout_timer);

    if (!has_confirmed)
    {
        (void)snprintf(out_reason, in_reason_size, "not confirmed at the board within %u s",
                       OTT_DISPLAY_TEST_TIMEOUT_MS / OTT_DISPLAY_TEST_MS_PER_SECOND);
    }

    return has_confirmed;
}
