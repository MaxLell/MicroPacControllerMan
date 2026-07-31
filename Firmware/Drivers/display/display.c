/*
 * Target implementation of the display port, on the X-NUCLEO-GFX01M2's ST7789V.
 *
 * A thin adapter: the controller driver already speaks RGB565 rectangles and the
 * frame buffer already stores RGB565, so this only maps one vocabulary onto the other
 * and owns the enable state.
 */
#include "display.h"

#include <stdbool.h>
#include <stddef.h>

#include "custom_assert.h"
#include "framebuffer.h"
#include "st7789.h"

/* ==========================================================================
 * display - private
 * ========================================================================= */

_Static_assert(FRAMEBUFFER_WIDTH == ST7789_WIDTH, "the frame buffer and the panel must agree on width");
_Static_assert(FRAMEBUFFER_HEIGHT == ST7789_HEIGHT, "the frame buffer and the panel must agree on height");

static bool g_is_initialized = false;
static bool g_is_enabled = false;

/* ==========================================================================
 * display - public
 * ========================================================================= */

void display_init(void)
{
    ASSERT(false == g_is_initialized);

    st7789_init();

    g_is_initialized = true;
    g_is_enabled = true;
}

void display_present(const framebuffer_t* in_framebuffer)
{
    ASSERT(g_is_initialized);
    ASSERT(in_framebuffer != NULL);

    /* The whole frame — 153,600 bytes, measured at 252 ms. Fine for a still image, far
     * too slow to animate: use display_present_region() for that and let the caller
     * decide what changed. See
     * [M2 Board Bring-Up §3](../../../Docu/Design/M2-Board-Bring-Up.md). */
    st7789_write_pixels(0U, 0U, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, framebuffer_get_line(in_framebuffer, 0),
                        FRAMEBUFFER_WIDTH);
}

void display_present_region(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                            int16_t in_height)
{
    ASSERT(g_is_initialized);
    ASSERT(in_framebuffer != NULL);
    ASSERT(in_x >= 0);
    ASSERT(in_y >= 0);
    ASSERT(in_width > 0);
    ASSERT(in_height > 0);
    ASSERT(in_x + in_width <= FRAMEBUFFER_WIDTH);
    ASSERT(in_y + in_height <= FRAMEBUFFER_HEIGHT);

    /* The stride stays the full buffer width, so the driver walks the rows of the
     * region without anything being copied first. */
    st7789_write_pixels((uint16_t)in_x, (uint16_t)in_y, (uint16_t)in_width, (uint16_t)in_height,
                        &framebuffer_get_line(in_framebuffer, in_y)[in_x], FRAMEBUFFER_WIDTH);
}

void display_clear(void)
{
    ASSERT(g_is_initialized);

    st7789_fill_screen(FRAMEBUFFER_COLOR_BLACK);
}

void display_set_enabled(bool in_is_enabled)
{
    ASSERT(g_is_initialized);

    /* The panel keeps its contents either way; only the output stage is switched. */
    st7789_set_display_on(in_is_enabled);

    g_is_enabled = in_is_enabled;
}

void display_service(void)
{
    ASSERT(g_is_initialized);

    /* Nothing to do. The port carries this for the Sharp memory LCD, whose liquid
     * crystal degrades without periodic COM inversion; the ST7789V needs no upkeep. */
}
