/*
 * touchpad.h
 *
 * Touchpad Click driver — Microchip MTCH6102 capacitive controller. The controller
 * boots in full mode with its default channel map, so a position can be read
 * without writing any configuration.
 *
 * Positions are the controller's raw decoded coordinates, not panel pixels;
 * mapping them onto a display is the caller's job.
 */

#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c_bsp.h"

/* ==========================================================================
 * touchpad - public types
 * ========================================================================= */

/*! \brief Largest coordinate the controller reports, per axis. */
#define TOUCHPAD_X_MAX (576U)
#define TOUCHPAD_Y_MAX (384U)

typedef struct
{
    uint16_t x;                                 /*!< Raw position, `0`..#TOUCHPAD_X_MAX       */
    uint16_t y;                                 /*!< Raw position, `0`..#TOUCHPAD_Y_MAX       */
    bool is_touched;                            /*!< A finger is present                      */
} touchpad_reading_t;

/* ==========================================================================
 * touchpad - public API
 * ========================================================================= */

/*! \brief Reset the controller and wait for it to finish booting. */
void touchpad_init(void);

/*! \brief Check that the controller acknowledges on the bus.
 *
 * \return          \ref I2C_BSP_STATUS_OK when the device answered, member of
 *                      \ref i2c_bsp_status_e otherwise
 */
i2c_bsp_status_e touchpad_probe(void);

/*! \brief Read the current touch state and position.
 *
 * The position is zero while no finger is present.
 *
 * \param[out]      out_reading: receives the touch state, must not be `NULL`
 * \return          \ref I2C_BSP_STATUS_OK on success, member of
 *                      \ref i2c_bsp_status_e otherwise
 */
i2c_bsp_status_e touchpad_read(touchpad_reading_t* out_reading);

#endif /* TOUCHPAD_H */
