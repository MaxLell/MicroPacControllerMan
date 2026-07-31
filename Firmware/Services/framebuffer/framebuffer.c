#include "framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"

/* ==========================================================================
 * framebuffer - private
 * ========================================================================= */

static bool prv_is_inside(int16_t in_x, int16_t in_y)
{
    return (in_x >= 0) && (in_x < FRAMEBUFFER_WIDTH) && (in_y >= 0)
           && (in_y < FRAMEBUFFER_HEIGHT);
}

/* ==========================================================================
 * framebuffer - public
 * ========================================================================= */

void framebuffer_clear(framebuffer_t* inout_framebuffer)
{
    framebuffer_fill(inout_framebuffer, FRAMEBUFFER_COLOR_WHITE);
}

void framebuffer_fill(framebuffer_t* inout_framebuffer, framebuffer_color_t in_color)
{
    ASSERT(inout_framebuffer != NULL);

    /* Not memset: a pixel is two bytes, and only colours whose halves happen to be
     * equal — black and white among them — would survive a byte-wise fill. */
    for (int16_t y = 0; y < FRAMEBUFFER_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < FRAMEBUFFER_WIDTH; ++x)
        {
            inout_framebuffer->pixels[y][x] = in_color;
        }
    }
}

void framebuffer_set_pixel(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                           framebuffer_color_t in_color)
{
    ASSERT(inout_framebuffer != NULL);

    if (prv_is_inside(in_x, in_y))
    {
        inout_framebuffer->pixels[in_y][in_x] = in_color;
    }
}

framebuffer_color_t framebuffer_get_pixel(const framebuffer_t* in_framebuffer, int16_t in_x,
                                          int16_t in_y)
{
    framebuffer_color_t color = FRAMEBUFFER_COLOR_WHITE;

    ASSERT(in_framebuffer != NULL);

    if (prv_is_inside(in_x, in_y))
    {
        color = in_framebuffer->pixels[in_y][in_x];
    }

    return color;
}

const framebuffer_color_t* framebuffer_get_line(const framebuffer_t* in_framebuffer, int16_t in_y)
{
    ASSERT(in_framebuffer != NULL);
    ASSERT(in_y >= 0);
    ASSERT(in_y < FRAMEBUFFER_HEIGHT);

    return in_framebuffer->pixels[in_y];
}
