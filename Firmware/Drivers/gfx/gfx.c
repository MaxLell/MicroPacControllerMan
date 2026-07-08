#include "gfx.h"

#include "display.h"

static void prv_swap(int* a, int* b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

void gfx_fill(int color)
{
    if (color == DISPLAY_WHITE) {
        display_clear();
    } else {
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                display_pixel(x, y, color);
            }
        }
    }
}

void gfx_hline(int x, int y, int w, int color)
{
    for (int i = 0; i < w; i++) {
        display_pixel(x + i, y, color);
    }
}

void gfx_vline(int x, int y, int h, int color)
{
    for (int i = 0; i < h; i++) {
        display_pixel(x, y + i, color);
    }
}

void gfx_line(int x0, int y0, int x1, int y1, int color)
{
    int steep = (y1 - y0 >= 0 ? y1 - y0 : y0 - y1) > (x1 - x0 >= 0 ? x1 - x0 : x0 - x1);
    if (steep) {
        prv_swap(&x0, &y0);
        prv_swap(&x1, &y1);
    }
    if (x0 > x1) {
        prv_swap(&x0, &x1);
        prv_swap(&y0, &y1);
    }
    int dx = x1 - x0;
    int dy = (y1 - y0 >= 0) ? (y1 - y0) : (y0 - y1);
    int err = dx / 2;
    int ystep = (y0 < y1) ? 1 : -1;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        if (steep) {
            display_pixel(y, x, color);
        } else {
            display_pixel(x, y, color);
        }
        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
}

void gfx_rect(int x, int y, int w, int h, int color)
{
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_fill_rect(int x, int y, int w, int h, int color)
{
    for (int j = 0; j < h; j++) {
        gfx_hline(x, y + j, w, color);
    }
}

void gfx_circle(int cx, int cy, int r, int color)
{
    int x = 0;
    int y = r;
    int d = 1 - r;
    while (x <= y) {
        display_pixel(cx + x, cy + y, color);
        display_pixel(cx - x, cy + y, color);
        display_pixel(cx + x, cy - y, color);
        display_pixel(cx - x, cy - y, color);
        display_pixel(cx + y, cy + x, color);
        display_pixel(cx - y, cy + x, color);
        display_pixel(cx + y, cy - x, color);
        display_pixel(cx - y, cy - x, color);
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

void gfx_fill_circle(int cx, int cy, int r, int color)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                display_pixel(cx + x, cy + y, color);
            }
        }
    }
}

void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    gfx_line(x0, y0, x1, y1, color);
    gfx_line(x1, y1, x2, y2, color);
    gfx_line(x2, y2, x0, y0, color);
}

/* Standard flat-top/flat-bottom scanline fill (as in Adafruit_GFX). */
void gfx_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    if (y0 > y1) {
        prv_swap(&y0, &y1);
        prv_swap(&x0, &x1);
    }
    if (y1 > y2) {
        prv_swap(&y2, &y1);
        prv_swap(&x2, &x1);
    }
    if (y0 > y1) {
        prv_swap(&y0, &y1);
        prv_swap(&x0, &x1);
    }

    if (y0 == y2) { /* degenerate: all on one line */
        int a = x0, b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        gfx_hline(a, y0, b - a + 1, color);
        return;
    }

    int dx01 = x1 - x0, dy01 = y1 - y0;
    int dx02 = x2 - x0, dy02 = y2 - y0;
    int dx12 = x2 - x1, dy12 = y2 - y1;
    long sa = 0, sb = 0;

    int last = (y1 == y2) ? y1 : (y1 - 1);
    int y;
    for (y = y0; y <= last; y++) {
        int a = x0 + (int)(sa / (dy01 ? dy01 : 1));
        int b = x0 + (int)(sb / dy02);
        sa += dx01;
        sb += dx02;
        if (a > b) prv_swap(&a, &b);
        gfx_hline(a, y, b - a + 1, color);
    }

    sa = (long)dx12 * (y - y1);
    sb = (long)dx02 * (y - y0);
    for (; y <= y2; y++) {
        int a = x1 + (int)(sa / (dy12 ? dy12 : 1));
        int b = x0 + (int)(sb / dy02);
        sa += dx12;
        sb += dx02;
        if (a > b) prv_swap(&a, &b);
        gfx_hline(a, y, b - a + 1, color);
    }
}
