#include "sprite.h"

#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "framebuffer.h"

/* ==========================================================================
 * sprite - private
 * ========================================================================= */

/* Which palette entry a row character stands for, or zero for "do not draw".
 *
 * Anything unrecognised is treated as transparent rather than asserted on: a stray
 * character in hand-edited art should leave a hole you can see, not stop the firmware. */
static uint8_t prv_get_color_index(char in_pixel)
{
    switch (in_pixel)
    {
        case SPRITE_CHAR_INDEX_1: return 1U;

        case SPRITE_CHAR_INDEX_2: return 2U;

        case SPRITE_CHAR_INDEX_3: return 3U;

        default: return 0U;
    }
}

/* ==========================================================================
 * sprite - public
 * ========================================================================= */

void sprite_draw(framebuffer_t* inout_framebuffer, const sprite_t* in_sprite, const sprite_palette_t* in_palette,
                 int16_t in_x, int16_t in_y)
{
    ASSERT(inout_framebuffer != NULL);
    ASSERT(in_sprite != NULL);
    ASSERT(in_sprite->rows != NULL);
    ASSERT(in_palette != NULL);

    for (uint8_t row = 0U; row < in_sprite->height; ++row)
    {
        const char* const pixels = in_sprite->rows[row];

        ASSERT(pixels != NULL);

        for (uint8_t column = 0U; column < in_sprite->width; ++column)
        {
            const uint8_t color_index = prv_get_color_index(pixels[column]);

            if (color_index == 0U)
            {
                continue;
            }

            framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_x + column), (int16_t)(in_y + row),
                                  in_palette->colors[color_index]);
        }
    }
}
