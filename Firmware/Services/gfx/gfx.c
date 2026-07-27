#include "gfx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "framebuffer.h"

/* ==========================================================================
 * gfx - private
 * ========================================================================= */

/* Guard against a zero divisor on a degenerate (zero-height) triangle edge. */
#define GFX_MIN_DIVISOR (1)

static void prv_swap(int16_t* inout_first, int16_t* inout_second)
{
    int16_t scratch;

    ASSERT(inout_first != NULL);
    ASSERT(inout_second != NULL);

    scratch = *inout_first;
    *inout_first = *inout_second;
    *inout_second = scratch;
}

static int16_t prv_absolute(int16_t in_value)
{
    return (in_value < 0) ? (int16_t)(-in_value) : in_value;
}

static int16_t prv_safe_divisor(int16_t in_divisor)
{
    return (in_divisor != 0) ? in_divisor : GFX_MIN_DIVISOR;
}

/* ==========================================================================
 * gfx - public
 * ========================================================================= */

void gfx_fill(framebuffer_t* inout_framebuffer, framebuffer_color_e in_color)
{
    framebuffer_fill(inout_framebuffer, in_color);
}

void gfx_horizontal_line(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                         int16_t in_width, framebuffer_color_e in_color)
{
    for (int16_t offset = 0; offset < in_width; ++offset)
    {
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_x + offset), in_y, in_color);
    }
}

void gfx_vertical_line(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                       int16_t in_height, framebuffer_color_e in_color)
{
    for (int16_t offset = 0; offset < in_height; ++offset)
    {
        framebuffer_set_pixel(inout_framebuffer, in_x, (int16_t)(in_y + offset), in_color);
    }
}

void gfx_line(framebuffer_t* inout_framebuffer, int16_t in_x_start, int16_t in_y_start,
              int16_t in_x_end, int16_t in_y_end, framebuffer_color_e in_color)
{
    int16_t x_start = in_x_start;
    int16_t y_start = in_y_start;
    int16_t x_end = in_x_end;
    int16_t y_end = in_y_end;
    int16_t delta_x;
    int16_t delta_y;
    int16_t error;
    int16_t y_step;
    int16_t y;
    bool is_steep;

    /* Bresenham only walks the shallow axis, so mirror a steep line into a shallow
     * one and swap the coordinates back when plotting. */
    is_steep = prv_absolute((int16_t)(y_end - y_start)) > prv_absolute((int16_t)(x_end - x_start));

    if (is_steep)
    {
        prv_swap(&x_start, &y_start);
        prv_swap(&x_end, &y_end);
    }

    if (x_start > x_end)
    {
        prv_swap(&x_start, &x_end);
        prv_swap(&y_start, &y_end);
    }

    delta_x = (int16_t)(x_end - x_start);
    delta_y = prv_absolute((int16_t)(y_end - y_start));
    error = (int16_t)(delta_x / 2);
    y_step = (y_start < y_end) ? 1 : -1;
    y = y_start;

    for (int16_t x = x_start; x <= x_end; ++x)
    {
        if (is_steep)
        {
            framebuffer_set_pixel(inout_framebuffer, y, x, in_color);
        }
        else
        {
            framebuffer_set_pixel(inout_framebuffer, x, y, in_color);
        }

        error = (int16_t)(error - delta_y);

        if (error < 0)
        {
            y = (int16_t)(y + y_step);
            error = (int16_t)(error + delta_x);
        }
    }
}

void gfx_rectangle(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y, int16_t in_width,
                   int16_t in_height, framebuffer_color_e in_color)
{
    gfx_horizontal_line(inout_framebuffer, in_x, in_y, in_width, in_color);
    gfx_horizontal_line(inout_framebuffer, in_x, (int16_t)(in_y + in_height - 1), in_width,
                        in_color);
    gfx_vertical_line(inout_framebuffer, in_x, in_y, in_height, in_color);
    gfx_vertical_line(inout_framebuffer, (int16_t)(in_x + in_width - 1), in_y, in_height, in_color);
}

void gfx_filled_rectangle(framebuffer_t* inout_framebuffer, int16_t in_x, int16_t in_y,
                          int16_t in_width, int16_t in_height, framebuffer_color_e in_color)
{
    for (int16_t row = 0; row < in_height; ++row)
    {
        gfx_horizontal_line(inout_framebuffer, in_x, (int16_t)(in_y + row), in_width, in_color);
    }
}

void gfx_circle(framebuffer_t* inout_framebuffer, int16_t in_center_x, int16_t in_center_y,
                int16_t in_radius, framebuffer_color_e in_color)
{
    int16_t x = 0;
    int16_t y = in_radius;
    int16_t decision = (int16_t)(1 - in_radius);

    /* Midpoint circle: walk one octant and mirror each point into the other seven. */
    while (x <= y)
    {
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x + x),
                              (int16_t)(in_center_y + y), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x - x),
                              (int16_t)(in_center_y + y), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x + x),
                              (int16_t)(in_center_y - y), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x - x),
                              (int16_t)(in_center_y - y), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x + y),
                              (int16_t)(in_center_y + x), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x - y),
                              (int16_t)(in_center_y + x), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x + y),
                              (int16_t)(in_center_y - x), in_color);
        framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x - y),
                              (int16_t)(in_center_y - x), in_color);

        ++x;

        if (decision < 0)
        {
            decision = (int16_t)(decision + (2 * x) + 1);
        }
        else
        {
            --y;
            decision = (int16_t)(decision + (2 * (x - y)) + 1);
        }
    }
}

void gfx_filled_circle(framebuffer_t* inout_framebuffer, int16_t in_center_x, int16_t in_center_y,
                       int16_t in_radius, framebuffer_color_e in_color)
{
    const int32_t radius_squared = (int32_t)in_radius * in_radius;

    for (int16_t y = (int16_t)(-in_radius); y <= in_radius; ++y)
    {
        for (int16_t x = (int16_t)(-in_radius); x <= in_radius; ++x)
        {
            if ((((int32_t)x * x) + ((int32_t)y * y)) <= radius_squared)
            {
                framebuffer_set_pixel(inout_framebuffer, (int16_t)(in_center_x + x),
                                      (int16_t)(in_center_y + y), in_color);
            }
        }
    }
}

void gfx_triangle(framebuffer_t* inout_framebuffer, int16_t in_x_0, int16_t in_y_0, int16_t in_x_1,
                  int16_t in_y_1, int16_t in_x_2, int16_t in_y_2, framebuffer_color_e in_color)
{
    gfx_line(inout_framebuffer, in_x_0, in_y_0, in_x_1, in_y_1, in_color);
    gfx_line(inout_framebuffer, in_x_1, in_y_1, in_x_2, in_y_2, in_color);
    gfx_line(inout_framebuffer, in_x_2, in_y_2, in_x_0, in_y_0, in_color);
}

void gfx_filled_triangle(framebuffer_t* inout_framebuffer, int16_t in_x_0, int16_t in_y_0,
                         int16_t in_x_1, int16_t in_y_1, int16_t in_x_2, int16_t in_y_2,
                         framebuffer_color_e in_color)
{
    int16_t x_top = in_x_0;
    int16_t y_top = in_y_0;
    int16_t x_middle = in_x_1;
    int16_t y_middle = in_y_1;
    int16_t x_bottom = in_x_2;
    int16_t y_bottom = in_y_2;
    int16_t delta_x_top_middle;
    int16_t delta_y_top_middle;
    int16_t delta_x_top_bottom;
    int16_t delta_y_top_bottom;
    int16_t delta_x_middle_bottom;
    int16_t delta_y_middle_bottom;
    int16_t left_x;
    int16_t right_x;
    int16_t last_flat_top_row;
    int16_t row;
    int32_t left_accumulator = 0;
    int32_t right_accumulator = 0;

    /* Sort the corners by row, so the shape splits into a flat-bottom half above the
     * middle corner and a flat-top half below it. */
    if (y_top > y_middle)
    {
        prv_swap(&y_top, &y_middle);
        prv_swap(&x_top, &x_middle);
    }

    if (y_middle > y_bottom)
    {
        prv_swap(&y_bottom, &y_middle);
        prv_swap(&x_bottom, &x_middle);
    }

    if (y_top > y_middle)
    {
        prv_swap(&y_top, &y_middle);
        prv_swap(&x_top, &x_middle);
    }

    if (y_top == y_bottom)
    {
        /* Degenerate: all three corners on one row, so draw their span. */
        left_x = x_top;
        right_x = x_top;

        if (x_middle < left_x)
        {
            left_x = x_middle;
        }
        else if (x_middle > right_x)
        {
            right_x = x_middle;
        }

        if (x_bottom < left_x)
        {
            left_x = x_bottom;
        }
        else if (x_bottom > right_x)
        {
            right_x = x_bottom;
        }

        gfx_horizontal_line(inout_framebuffer, left_x, y_top, (int16_t)(right_x - left_x + 1),
                            in_color);

        return;
    }

    delta_x_top_middle = (int16_t)(x_middle - x_top);
    delta_y_top_middle = (int16_t)(y_middle - y_top);
    delta_x_top_bottom = (int16_t)(x_bottom - x_top);
    delta_y_top_bottom = (int16_t)(y_bottom - y_top);
    delta_x_middle_bottom = (int16_t)(x_bottom - x_middle);
    delta_y_middle_bottom = (int16_t)(y_bottom - y_middle);

    /* A flat-bottom triangle includes the middle row; otherwise it is drawn by the
     * second loop. */
    last_flat_top_row = (y_middle == y_bottom) ? y_middle : (int16_t)(y_middle - 1);

    for (row = y_top; row <= last_flat_top_row; ++row)
    {
        left_x = (int16_t)(x_top + (left_accumulator / prv_safe_divisor(delta_y_top_middle)));
        right_x = (int16_t)(x_top + (right_accumulator / prv_safe_divisor(delta_y_top_bottom)));

        left_accumulator += delta_x_top_middle;
        right_accumulator += delta_x_top_bottom;

        if (left_x > right_x)
        {
            prv_swap(&left_x, &right_x);
        }

        gfx_horizontal_line(inout_framebuffer, left_x, row, (int16_t)(right_x - left_x + 1),
                            in_color);
    }

    left_accumulator = (int32_t)delta_x_middle_bottom * (row - y_middle);
    right_accumulator = (int32_t)delta_x_top_bottom * (row - y_top);

    for (; row <= y_bottom; ++row)
    {
        left_x = (int16_t)(x_middle + (left_accumulator / prv_safe_divisor(delta_y_middle_bottom)));
        right_x = (int16_t)(x_top + (right_accumulator / prv_safe_divisor(delta_y_top_bottom)));

        left_accumulator += delta_x_middle_bottom;
        right_accumulator += delta_x_top_bottom;

        if (left_x > right_x)
        {
            prv_swap(&left_x, &right_x);
        }

        gfx_horizontal_line(inout_framebuffer, left_x, row, (int16_t)(right_x - left_x + 1),
                            in_color);
    }
}
