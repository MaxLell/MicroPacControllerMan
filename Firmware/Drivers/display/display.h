#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * LCD Mono Click driver — Sharp LS013B7DH03, 128x128, 1 bpp memory LCD, over SPI1
 * (Bsp/spi). Write-only panel with a RAM framebuffer here; nothing appears until
 * display_flush(). Protocol: mode 0, LSB-first, active-HIGH CS, ~1 MHz.
 *
 * Control lines (LCD Mono Click, mikroBUS slot 1 — R-001, to confirm on hardware):
 *   CS       = PB6  (GPIO, active HIGH)
 *   DISP     = PA6  (GPIO, high = panel on) — the Click routes DISP onto the
 *                    mikroBUS MISO line, so PA6 is used as GPIO, not SPI MISO
 *   EXTCOMIN = PB10 (GPIO, mikroBUS PWM) — external VCOM clock
 *
 * VCOM/polarity inversion: this driver drives the software VCOM bit on every
 * flush AND toggles the EXTCOMIN pin on display_vcom_tick(), so it works whether
 * the board's EXTMODE jumper selects software or external COM inversion. Call
 * display_vcom_tick() at ~1 Hz while a static image is held (Sharp requires an
 * inversion at least once per second).
 *
 * Colors are drawing-sense: BLACK = ink on, WHITE = background.
 */

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 128

#define DISPLAY_WHITE 0
#define DISPLAY_BLACK 1

void display_init(void);                 /* SPI + GPIOs, panel on, cleared */
void display_clear(void);                /* framebuffer -> all white (no flush) */
void display_pixel(int x, int y, int color); /* set one pixel in the framebuffer */
void display_flush(void);                /* push the whole framebuffer to the panel */
void display_all_clear(void);            /* hardware clear-all command (+ framebuffer) */
void display_on(int on);                 /* DISP pin: 1 = on, 0 = blank (RAM kept) */
void display_vcom_tick(void);            /* service VCOM while idle (call ~1 Hz) */

#endif /* DISPLAY_H */
