/*
 * gfx.h
 *
 * 1-bpp geometric primitives that draw into a frame buffer: lines, rectangles,
 * circles, triangles, filled and outlined. No text or logo.
 *
 * Pure logic with no hardware behind it — a caller draws into a #framebuffer_t and
 * hands that to a display when it wants it shown. Shapes may extend past the buffer
 * edges; the frame buffer clips them.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#include "framebuffer.h"

/* ==========================================================================
 * gfx - public API
 * ========================================================================= */

/*! \brief Fill the whole buffer with one colour. */
void gfx_fill(framebuffer_t* inout_framebuffer, framebuffer_color_e in_color);

/*! \brief Draw a horizontal line of `in_width` pixels, starting at the given point. */
void gfx_horizontal_line(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                         int16_t in_width, framebuffer_color_e in_color);

/*! \brief Draw a vertical line of `in_height` pixels, starting at the given point. */
void gfx_vertical_line(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                       int16_t in_height, framebuffer_color_e in_color);

/*! \brief Draw a line between two points (Bresenham). */
void gfx_line(framebuffer_t* inout_framebuffer, int16_t in_x_start, int16_t in_y_start,
              int16_t in_x_end, int16_t in_y_end, framebuffer_color_e in_color);

/*! \brief Draw the outline of a rectangle, `in_x`/`in_y` being its top-left corner. */
void gfx_rectangle(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                   int16_t in_height, framebuffer_color_e in_color);

/*! \brief Draw a filled rectangle, `in_x`/`in_y` being its top-left corner. */
void gfx_filled_rectangle(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                          int16_t in_width, int16_t in_height, framebuffer_color_e in_color);

/*! \brief Draw the outline of a circle around a centre point. */
void gfx_circle(framebuffer_t* inout_framebuffer, int16_t in_center_x, int16_t in_center_y,
                int16_t in_radius, framebuffer_color_e in_color);

/*! \brief Draw a filled circle around a centre point. */
void gfx_filled_circle(framebuffer_t* inout_framebuffer, int16_t in_center_x, int16_t in_center_y,
                       int16_t in_radius, framebuffer_color_e in_color);

/*! \brief Draw the outline of a triangle through three corners. */
void gfx_triangle(framebuffer_t* inout_framebuffer, int16_t in_x_0, int16_t in_y_0, int16_t in_x_1,
                  int16_t in_y_1, int16_t in_x_2, int16_t in_y_2, framebuffer_color_e in_color);

/*! \brief Draw a filled triangle through three corners. */
void gfx_filled_triangle(framebuffer_t* inout_framebuffer, int16_t in_x_0, int16_t in_y_0,
                         int16_t in_x_1, int16_t in_y_1, int16_t in_x_2, int16_t in_y_2,
                         framebuffer_color_e in_color);

#endif /* GFX_H */
