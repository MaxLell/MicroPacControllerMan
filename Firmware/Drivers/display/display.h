/*
 * display.h
 *
 * Display sink: shows a frame buffer, and owns nothing else.
 *
 * This is a platform port — one interface, one implementation per platform, selected
 * in CMakeLists.txt. `display.c` drives the GFX01M2's ST7789V panel over SPI;
 * `display_host.c` keeps the last frame in memory so a host build can inspect or
 * render it. Callers see no difference.
 *
 * The caller owns the frame buffer, so it can keep more than one and hand on a
 * snapshot ([03 §3.2](../../../Docu/PrePlanning/03-Architecture.md), R-007).
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

#include "framebuffer.h"

/* ==========================================================================
 * display - public API
 * ========================================================================= */

/*! \brief Longest interval at which #display_service must be called while an
 *         unchanging image is shown, in milliseconds. */
#define DISPLAY_SERVICE_PERIOD_MS (1000U)

/*! \brief Bring the display up: enabled, blank, ready to be presented to. */
void display_init(void);

/*! \brief Show a frame buffer.
 *
 * \param[in]       in_framebuffer: frame to show, must not be `NULL`. Borrowed only —
 *                      the display does not keep the pointer.
 */
void display_present(const framebuffer_t* in_framebuffer);

/*! \brief Show one rectangle of a frame buffer, leaving the rest of the panel alone.
 *
 * The lever that makes a usable frame rate possible. A whole frame is 153,600 bytes and
 * the transfer dominates everything else, so redrawing all of it to move a few sprites
 * is the cost that matters. An 8 x 8 cell is 128 bytes.
 *
 * The caller decides what changed; this only sends it.
 *
 * \param[in]       in_framebuffer: frame to read from, must not be `NULL`
 * \param[in]       in_x: left edge, below #FRAMEBUFFER_WIDTH
 * \param[in]       in_y: top edge, below #FRAMEBUFFER_HEIGHT
 * \param[in]       in_width: at least `1`, and `in_x + in_width` at most #FRAMEBUFFER_WIDTH
 * \param[in]       in_height: at least `1`, and `in_y + in_height` at most #FRAMEBUFFER_HEIGHT
 */
void display_present_region(const framebuffer_t* in_framebuffer, int16_t in_x, int16_t in_y,
                            int16_t in_width, int16_t in_height);

/*! \brief Blank the display itself, without touching any frame buffer. */
void display_clear(void);

/*! \brief Enable or blank the display. Blanking keeps what was last presented.
 *
 * \param[in]       in_is_enabled: `true` shows the image, `false` blanks it
 */
void display_set_enabled(bool in_is_enabled);

/*! \brief Periodic upkeep the platform may need.
 *
 * Must be called at least every #DISPLAY_SERVICE_PERIOD_MS while an unchanging image
 * is shown. On the target this services the panel's COM inversion, which the liquid
 * crystal needs to avoid degrading; on a host it does nothing.
 */
void display_service(void);

#endif /* DISPLAY_H */
