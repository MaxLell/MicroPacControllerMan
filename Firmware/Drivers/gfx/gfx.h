/*
 * gfx.h
 *
 * 1 bpp geometric primitives on top of the display frame buffer. Drawing only
 * touches the frame buffer — call display_flush() to make the result visible.
 * Shapes may extend past the panel edges; the display clips them.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#include "display.h"

/* ==========================================================================
 * gfx - public API
 * ========================================================================= */

/*! \brief Fill the whole frame buffer with one colour. */
void gfx_fill(display_color_e in_color);

/*! \brief Draw a horizontal line of `in_width` pixels, starting at the given point. */
void gfx_horizontal_line(int16_t in_x, int16_t in_y, int16_t in_width, display_color_e in_color);

/*! \brief Draw a vertical line of `in_height` pixels, starting at the given point. */
void gfx_vertical_line(int16_t in_x, int16_t in_y, int16_t in_height, display_color_e in_color);

/*! \brief Draw a line between two points (Bresenham). */
void gfx_line(int16_t in_x_start, int16_t in_y_start, int16_t in_x_end, int16_t in_y_end,
              display_color_e in_color);

/*! \brief Draw the outline of a rectangle, `in_x`/`in_y` being its top-left corner. */
void gfx_rectangle(int16_t in_x, int16_t in_y, int16_t in_width, int16_t in_height,
                   display_color_e in_color);

/*! \brief Draw a filled rectangle, `in_x`/`in_y` being its top-left corner. */
void gfx_filled_rectangle(int16_t in_x, int16_t in_y, int16_t in_width, int16_t in_height,
                          display_color_e in_color);

/*! \brief Draw the outline of a circle around a centre point. */
void gfx_circle(int16_t in_center_x, int16_t in_center_y, int16_t in_radius,
                display_color_e in_color);

/*! \brief Draw a filled circle around a centre point. */
void gfx_filled_circle(int16_t in_center_x, int16_t in_center_y, int16_t in_radius,
                       display_color_e in_color);

/*! \brief Draw the outline of a triangle through three corners. */
void gfx_triangle(int16_t in_x_0, int16_t in_y_0, int16_t in_x_1, int16_t in_y_1, int16_t in_x_2,
                  int16_t in_y_2, display_color_e in_color);

/*! \brief Draw a filled triangle through three corners. */
void gfx_filled_triangle(int16_t in_x_0, int16_t in_y_0, int16_t in_x_1, int16_t in_y_1,
                         int16_t in_x_2, int16_t in_y_2, display_color_e in_color);

#endif /* GFX_H */
