/*
 * st7789.h
 *
 * Driver for the ST7789V controller of the X-NUCLEO-GFX01M2's 240x320 panel.
 *
 * This is the controller driver, not the display *port*: it speaks in pixels and
 * rectangles and knows the panel's quirks. `Drivers/display` is the port the game
 * sees, and will be built on top of this once the colour frame buffer exists.
 *
 * Colours are RGB565, the controller's 16-bit pixel format — five bits red, six
 * green, five blue, sent most-significant byte first.
 */

#ifndef ST7789_H
#define ST7789_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * st7789 - public types
 * ========================================================================= */

/*! \brief Panel geometry in its native, unrotated orientation. */
#define ST7789_WIDTH (240U)
#define ST7789_HEIGHT (320U)

/*! \brief Bytes the controller returns for its identity: manufacturer, version, id. */
#define ST7789_ID_LENGTH (3U)

/*! \brief The identity an ST7789V reports, so a driver can check what it is talking
 *         to rather than trusting the label on the board. */
#define ST7789_ID_EXPECTED_0 (0x85U)
#define ST7789_ID_EXPECTED_1 (0x85U)
#define ST7789_ID_EXPECTED_2 (0x52U)

/*! \brief Build an RGB565 colour from 8-bit components. */
#define ST7789_RGB(red, green, blue)                                                     \
    ((uint16_t)((((uint16_t)(red) & 0xF8U) << 8) | (((uint16_t)(green) & 0xFCU) << 3)     \
                | (((uint16_t)(blue) & 0xF8U) >> 3)))

/* ==========================================================================
 * st7789 - public API
 * ========================================================================= */

/*! \brief Reset the panel and bring it up: 16-bit pixels, display on, blanked.
 *
 * Must be called once before any other function of this module. Leaves the screen
 * filled black rather than showing whatever the RAM happened to contain.
 */
void st7789_init(void);

/*! \brief Read the controller's identity.
 *
 * \param[out]      out_id: receives #ST7789_ID_LENGTH bytes, must not be `NULL`
 */
void st7789_read_id(uint8_t* out_id);

/*! \brief Whether the identity matches an ST7789V.
 *
 * A cheap start-up self-check: a wrong or absent controller answers with zeroes.
 */
bool st7789_is_present(void);

/*! \brief Fill a rectangle with one colour.
 *
 * \param[in]       in_x: left edge, below #ST7789_WIDTH
 * \param[in]       in_y: top edge, below #ST7789_HEIGHT
 * \param[in]       in_width: at least `1`, and `in_x + in_width` at most #ST7789_WIDTH
 * \param[in]       in_height: at least `1`, and `in_y + in_height` at most #ST7789_HEIGHT
 * \param[in]       in_colour: RGB565 colour
 */
void st7789_fill_rectangle(uint16_t in_x, uint16_t in_y, uint16_t in_width, uint16_t in_height,
                           uint16_t in_colour);

/*! \brief Fill the whole screen with one colour. */
void st7789_fill_screen(uint16_t in_colour);

#endif /* ST7789_H */
