/*
 * framebuffer.h
 *
 * An off-screen colour image: memory plus the arithmetic to address it, and nothing
 * else. Platform-independent, so drawing can be built and unit-tested on the host;
 * presenting one of these is a display driver's only job.
 *
 * Pixels are **RGB565** — five bits red, six green, five blue — which is the format
 * the panel's controller consumes, so nothing is converted on the way out.
 *
 * A whole frame is 240 x 320 x 2 bytes = **153,600 bytes**, 60 % of the
 * microcontroller's contiguous SRAM. So a `framebuffer_t` belongs in static storage:
 * one will not fit on a stack, and two will not fit at all. That rules out the
 * double-buffered snapshot [03 §3.2](../../../Docu/PrePlanning/03-Architecture.md)
 * assumed when a frame was 2 kB — see [M2 Board Bring-Up §3](../../../Docu/Design/M2-Board-Bring-Up.md).
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

/* ==========================================================================
 * framebuffer - public types
 * ========================================================================= */

#define FRAMEBUFFER_WIDTH  (240)
#define FRAMEBUFFER_HEIGHT (320)

/*! \brief One pixel, RGB565. */
typedef uint16_t framebuffer_color_t;

/*! \brief Build a colour from 8-bit components; the low bits are discarded. */
#define FRAMEBUFFER_RGB(red, green, blue)                                                                              \
    ((framebuffer_color_t)((((uint16_t)(red) & 0xF8U) << 8) | (((uint16_t)(green) & 0xFCU) << 3)                       \
                           | (((uint16_t)(blue) & 0xF8U) >> 3)))

#define FRAMEBUFFER_COLOR_BLACK   FRAMEBUFFER_RGB(0U, 0U, 0U)
#define FRAMEBUFFER_COLOR_WHITE   FRAMEBUFFER_RGB(255U, 255U, 255U)
#define FRAMEBUFFER_COLOR_RED     FRAMEBUFFER_RGB(255U, 0U, 0U)
#define FRAMEBUFFER_COLOR_GREEN   FRAMEBUFFER_RGB(0U, 255U, 0U)
#define FRAMEBUFFER_COLOR_BLUE    FRAMEBUFFER_RGB(0U, 0U, 255U)
#define FRAMEBUFFER_COLOR_YELLOW  FRAMEBUFFER_RGB(255U, 255U, 0U)
#define FRAMEBUFFER_COLOR_CYAN    FRAMEBUFFER_RGB(0U, 255U, 255U)
#define FRAMEBUFFER_COLOR_MAGENTA FRAMEBUFFER_RGB(255U, 0U, 255U)

typedef struct
{
    /*!< Row-major, one RGB565 value per pixel. */
    framebuffer_color_t pixels[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_WIDTH];
} framebuffer_t;

/* ==========================================================================
 * framebuffer - public API
 * ========================================================================= */

/*! \brief Set every pixel to white.
 *
 * The background this module has always cleared to. White is *bright* on an emissive
 * panel, so a game wanting a dark screen fills \ref FRAMEBUFFER_COLOR_BLACK instead
 * of clearing.
 *
 * \param[out]      inout_framebuffer: buffer to clear, must not be `NULL`
 */
void framebuffer_clear(framebuffer_t* inout_framebuffer);

/*! \brief Set every pixel to one colour.
 *
 * \param[out]      inout_framebuffer: buffer to fill, must not be `NULL`
 * \param[in]       in_color: colour to write
 */
void framebuffer_fill(framebuffer_t* inout_framebuffer, framebuffer_color_t in_color);

/*! \brief Write one pixel. Coordinates outside the buffer are ignored, so a caller
 *         may clip lazily.
 *
 * \param[in,out]   inout_framebuffer: buffer to draw into, must not be `NULL`
 * \param[in]       in_x: column, `0` is the left edge
 * \param[in]       in_y: row, `0` is the top edge
 * \param[in]       in_color: colour to write
 */
void framebuffer_set_pixel(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y, framebuffer_color_t in_color);

/*! \brief Read one pixel.
 *
 * \param[in]       in_framebuffer: buffer to read, must not be `NULL`
 * \param[in]       in_x: column
 * \param[in]       in_y: row
 * \return          The pixel, or \ref FRAMEBUFFER_COLOR_WHITE outside the buffer
 */
framebuffer_color_t framebuffer_get_pixel(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y);

/*! \brief Borrow one row, for a display driver that pushes whole lines.
 *
 * \param[in]       in_framebuffer: buffer to read, must not be `NULL`
 * \param[in]       in_y: row, must be inside the buffer
 * \return          Pointer to \ref FRAMEBUFFER_WIDTH pixels, owned by the buffer
 */
const framebuffer_color_t* framebuffer_get_line(const framebuffer_t* in_framebuffer, int16_t in_y);

#endif /* FRAMEBUFFER_H */
