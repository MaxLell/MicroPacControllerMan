/*
 * framebuffer.h
 *
 * A 1-bit-per-pixel frame buffer: memory plus the bit arithmetic to address it, and
 * nothing else. Platform-independent, so drawing can be built and unit-tested on the
 * host; presenting one of these is a display driver's only job.
 *
 * Colours are logical, not panel-native — a set bit means ink. Whatever bit polarity
 * a particular panel wants is that driver's problem, not the caller's.
 *
 * The buffer is an object rather than a hidden global so several can exist: the
 * render path is specified to hand on a double-buffered snapshot
 * ([03 §3.2](../../../Docu/PrePlanning/03-Architecture.md), R-007).
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

/* ==========================================================================
 * framebuffer - public types
 * ========================================================================= */

#define FRAMEBUFFER_WIDTH          (128)
#define FRAMEBUFFER_HEIGHT         (128)

#define FRAMEBUFFER_BITS_PER_BYTE  (8)
#define FRAMEBUFFER_BYTES_PER_LINE (FRAMEBUFFER_WIDTH / FRAMEBUFFER_BITS_PER_BYTE)

typedef enum
{
    FRAMEBUFFER_COLOR_WHITE = 0, /*!< background — no ink              */
    FRAMEBUFFER_COLOR_BLACK      /*!< ink on                           */
} framebuffer_color_e;

typedef struct
{
    /*!< One bit per pixel. Bit 0 of a byte is its left-most pixel; a set bit is ink. */
    uint8_t lines[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_BYTES_PER_LINE];
} framebuffer_t;

/* ==========================================================================
 * framebuffer - public API
 * ========================================================================= */

/*! \brief Set every pixel to white.
 *
 * \param[out]      inout_framebuffer: buffer to clear, must not be `NULL`
 */
void framebuffer_clear(framebuffer_t* inout_framebuffer);

/*! \brief Set every pixel to one colour.
 *
 * \param[out]      inout_framebuffer: buffer to fill, must not be `NULL`
 * \param[in]       in_color: member of \ref framebuffer_color_e
 */
void framebuffer_fill(framebuffer_t* inout_framebuffer, framebuffer_color_e in_color);

/*! \brief Set one pixel.
 *
 * Coordinates outside the buffer are ignored, so callers may draw over the edge and
 * let the clipping happen here.
 *
 * \param[in,out]   inout_framebuffer: buffer to draw into, must not be `NULL`
 * \param[in]       in_x: column, `0` is the left edge
 * \param[in]       in_y: row, `0` is the top edge
 * \param[in]       in_color: member of \ref framebuffer_color_e
 */
void framebuffer_set_pixel(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y, framebuffer_color_e in_color);

/*! \brief Read one pixel.
 *
 * \param[in]       in_framebuffer: buffer to read, must not be `NULL`
 * \param[in]       in_x: column
 * \param[in]       in_y: row
 * \return          The pixel's colour, or \ref FRAMEBUFFER_COLOR_WHITE for
 *                      coordinates outside the buffer
 */
framebuffer_color_e framebuffer_get_pixel(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y);

/*! \brief Borrow one row's packed bits, for a driver pushing the buffer out.
 *
 * \param[in]       in_framebuffer: buffer to read, must not be `NULL`
 * \param[in]       in_y: row, must be inside the buffer
 * \return          Pointer to #FRAMEBUFFER_BYTES_PER_LINE bytes owned by the buffer
 */
const uint8_t* framebuffer_get_line(const framebuffer_t* in_framebuffer, int16_t in_y);

#endif /* FRAMEBUFFER_H */
