#include "framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"

/* ==========================================================================
 * framebuffer - private
 * ========================================================================= */

#define FRAMEBUFFER_ALL_INK (0xFFU)
#define FRAMEBUFFER_NO_INK (0x00U)

static bool prv_is_inside(int16_t in_x, int16_t in_y)
{
    return (in_x >= 0) && (in_x < FRAMEBUFFER_WIDTH) && (in_y >= 0)
           && (in_y < FRAMEBUFFER_HEIGHT);
}

static uint8_t prv_bit_mask(int16_t in_x)
{
    return (uint8_t)(1U << (in_x % FRAMEBUFFER_BITS_PER_BYTE));
}

/* ==========================================================================
 * framebuffer - public
 * ========================================================================= */

void framebuffer_clear(framebuffer_t* inout_framebuffer)
{
    framebuffer_fill(inout_framebuffer, FRAMEBUFFER_COLOR_WHITE);
}

void framebuffer_fill(framebuffer_t* inout_framebuffer, framebuffer_color_e in_color)
{
    ASSERT(inout_framebuffer != NULL);

    memset(inout_framebuffer->lines,
           (in_color == FRAMEBUFFER_COLOR_BLACK) ? FRAMEBUFFER_ALL_INK : FRAMEBUFFER_NO_INK,
           sizeof(inout_framebuffer->lines));
}

void framebuffer_set_pixel(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                           framebuffer_color_e in_color)
{
    ASSERT(inout_framebuffer != NULL);

    if (!prv_is_inside(in_x, in_y))
    {
        return;
    }

    if (in_color == FRAMEBUFFER_COLOR_BLACK)
    {
        inout_framebuffer->lines[in_y][in_x / FRAMEBUFFER_BITS_PER_BYTE] |= prv_bit_mask(in_x);
    }
    else
    {
        inout_framebuffer->lines[in_y][in_x / FRAMEBUFFER_BITS_PER_BYTE]
            &= (uint8_t)~prv_bit_mask(in_x);
    }
}

framebuffer_color_e framebuffer_get_pixel(const framebuffer_t* in_framebuffer, int16_t in_x,
                                          int16_t in_y)
{
    ASSERT(in_framebuffer != NULL);

    if (!prv_is_inside(in_x, in_y))
    {
        return FRAMEBUFFER_COLOR_WHITE;
    }

    if ((in_framebuffer->lines[in_y][in_x / FRAMEBUFFER_BITS_PER_BYTE] & prv_bit_mask(in_x)) != 0U)
    {
        return FRAMEBUFFER_COLOR_BLACK;
    }

    return FRAMEBUFFER_COLOR_WHITE;
}

const uint8_t* framebuffer_get_line(const framebuffer_t* in_framebuffer, int16_t in_y)
{
    ASSERT(in_framebuffer != NULL);
    ASSERT(in_y >= 0);
    ASSERT(in_y < FRAMEBUFFER_HEIGHT);

    return in_framebuffer->lines[in_y];
}
