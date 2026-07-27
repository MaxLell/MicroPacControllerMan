/*
 * display.h
 *
 * LCD Mono Click driver — Sharp LS013B7DH03, 128x128, 1 bpp memory LCD. The panel
 * is write-only, so pixels go into a RAM frame buffer and nothing appears until
 * display_flush().
 *
 * The panel needs its COM polarity inverted at least once per second while a
 * static image is held, otherwise the liquid crystal degrades. The driver serves
 * both inversion modes on every display_service_vcom() call, so either setting of
 * the Click's mode-select jumper works.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * display - public types
 * ========================================================================= */

#define DISPLAY_WIDTH (128)
#define DISPLAY_HEIGHT (128)

/*! \brief Longest interval at which #display_service_vcom must be called while a
 *         static image is held, in milliseconds. */
#define DISPLAY_VCOM_PERIOD_MS (1000U)

/*! \brief Drawing colours. `BLACK` is ink on, `WHITE` is background. */
typedef enum
{
    DISPLAY_COLOR_WHITE = 0,
    DISPLAY_COLOR_BLACK
} display_color_e;

/* ==========================================================================
 * display - public API
 * ========================================================================= */

/*! \brief Bring the panel up: enable it, clear it, and leave it ready to draw. */
void display_init(void);

/*! \brief Set the whole frame buffer to white. Does not touch the panel. */
void display_clear(void);

/*! \brief Set one pixel in the frame buffer.
 *
 * Coordinates outside the panel are ignored, so callers may draw over the edge.
 *
 * \param[in]       in_x: column, `0` is the left edge
 * \param[in]       in_y: row, `0` is the top edge
 * \param[in]       in_color: member of \ref display_color_e
 */
void display_set_pixel(int16_t in_x, int16_t in_y, display_color_e in_color);

/*! \brief Push the whole frame buffer to the panel. */
void display_flush(void);

/*! \brief Clear the panel and the frame buffer with the panel's clear-all command. */
void display_clear_all(void);

/*! \brief Enable or blank the panel. Blanking keeps the panel's own memory.
 *
 * \param[in]       in_is_enabled: `true` shows the image, `false` blanks it
 */
void display_set_enabled(bool in_is_enabled);

/*! \brief Service the panel's COM inversion.
 *
 * Must be called at least every #DISPLAY_VCOM_PERIOD_MS while a static image is
 * held. Serves both software and external COM inversion.
 */
void display_service_vcom(void);

#endif /* DISPLAY_H */
