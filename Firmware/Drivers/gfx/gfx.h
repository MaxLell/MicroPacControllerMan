#ifndef GFX_H
#define GFX_H

/*
 * Minimal 1-bpp graphics primitives on top of the display framebuffer
 * (Drivers/display). No text/logo — just the geometric shapes used by the
 * display bring-up demo (VT-INT-006) and the touch-dot test. `color` is
 * DISPLAY_BLACK / DISPLAY_WHITE. Call display_flush() to make it visible.
 */

void gfx_fill(int color);
void gfx_hline(int x, int y, int w, int color);
void gfx_vline(int x, int y, int h, int color);
void gfx_line(int x0, int y0, int x1, int y1, int color);
void gfx_rect(int x, int y, int w, int h, int color);
void gfx_fill_rect(int x, int y, int w, int h, int color);
void gfx_circle(int cx, int cy, int r, int color);
void gfx_fill_circle(int cx, int cy, int r, int color);
void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);
void gfx_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);

#endif /* GFX_H */
