/*
 * sprite.h
 *
 * A small indexed picture drawn into a frame buffer, with transparency and a palette
 * chosen at the moment of drawing.
 *
 * Pure logic with no hardware behind it, like `gfx` — a caller draws into a
 * #framebuffer_t and hands that to a display when it wants it shown.
 *
 * **A sprite is text.** Each pixel is one character of one row string, which means the
 * art is readable and editable in the source with no tooling and no conversion step, the
 * way the maze already is in `playfield.c`. It costs four bytes per pixel instead of the
 * two bits a packed format would need — about 400 bytes for a 20 x 20 frame against 100.
 * With 90 % of the flash free that is a good trade; if it ever stops being one, packing
 * these into 2 bpp is a change to this module and to nothing else, because no caller
 * looks inside a #sprite_t.
 *
 * **The palette is not part of the sprite.** One ghost drawing plus four palettes is
 * Blinky, Pinky, Inky and Clyde; a fifth makes the frightened one blue and leaves the
 * eyes white. That is the whole reason for indexing rather than storing colours.
 */

#ifndef SPRITE_H
#define SPRITE_H

#include <stdint.h>

#include "framebuffer.h"

/* ==========================================================================
 * sprite - public types
 * ========================================================================= */

/*! \brief Characters a sprite row may contain. Index `0` is not drawn at all, which is
 *         what lets a round Pacman sit on a pellet without erasing the corners. */
#define SPRITE_CHAR_TRANSPARENT ('.')
#define SPRITE_CHAR_INDEX_1     ('1')
#define SPRITE_CHAR_INDEX_2     ('2')
#define SPRITE_CHAR_INDEX_3     ('3')

/*! \brief Colours a sprite can use, transparency included. Three drawn colours is what a
 *         ghost needs: body, eye white, pupil. */
#define SPRITE_COLOR_COUNT      (4U)

typedef struct
{
    uint8_t width;
    uint8_t height;

    /*!< `height` strings of `width` characters, each a \ref SPRITE_CHAR_TRANSPARENT or
     *   an index character. Not NUL-checked past `width` — the length is the contract. */
    const char* const* rows;
} sprite_t;

/*! \brief The colours the index characters stand for. Entry `0` is never read; it is
 *         there so the array index matches the character. */
typedef struct
{
    framebuffer_color_t colors[SPRITE_COLOR_COUNT];
} sprite_palette_t;

/* ==========================================================================
 * sprite - public API
 * ========================================================================= */

/*! \brief Draw a sprite into a frame buffer at a pixel position.
 *
 * Transparent pixels are left alone, so what was already in the buffer shows through.
 * Pixels outside the buffer are clipped by \ref framebuffer_set_pixel, which means a
 * sprite may hang off any edge — an actor entering through a tunnel does.
 *
 * \param[in,out]   inout_framebuffer: buffer to draw into, must not be `NULL`
 * \param[in]       in_sprite: the drawing, must not be `NULL`
 * \param[in]       in_palette: colours for its index characters, must not be `NULL`
 * \param[in]       in_x: left edge, in pixels
 * \param[in]       in_y: top edge, in pixels
 */
void sprite_draw(framebuffer_t* inout_framebuffer, const sprite_t* in_sprite, const sprite_palette_t* in_palette,
                 int16_t in_x, int16_t in_y);

#endif /* SPRITE_H */
