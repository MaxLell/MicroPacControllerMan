/*
 * dio_bsp.h
 *
 * Digital I/O abstraction. The only module allowed to touch HAL GPIO: every
 * other module addresses a pin by its logical name from #dio_bsp_pin_e.
 */

#ifndef DIO_BSP_H
#define DIO_BSP_H

/* ==========================================================================
 * dio_bsp - public types
 * ========================================================================= */

typedef enum
{
    DIO_BSP_PIN_NONE = 0,
    DIO_BSP_PIN_USER_BUTTON, /* PC13 - on-board user button B1, active HIGH */
    DIO_BSP_PIN_LED_GREEN,   /* PA5  - on-board LED LD2, active HIGH        */
    DIO_BSP_PIN_LAST
} dio_bsp_pin_e;

typedef enum
{
    DIO_BSP_PIN_STATE_LOW = 0,
    DIO_BSP_PIN_STATE_HIGH
} dio_bsp_pin_state_e;

/* ==========================================================================
 * dio_bsp - public API
 * ========================================================================= */

/*! \brief Initialize the digital I/O abstraction.
 *
 * Port clocks, pin modes and pull resistors are applied by the STM32CubeMX
 * MX_GPIO_Init(), which runs before app_main(). Must be called exactly once,
 * before any other function of this module.
 */
void dio_bsp_init(void);

/*! \brief Drive one output pin to the requested level.
 *
 * \param[in]       in_pin: pin to drive, a member of \ref dio_bsp_pin_e
 * \param[in]       in_state: level to drive, a member of \ref dio_bsp_pin_state_e
 */
void dio_bsp_set_pin(dio_bsp_pin_e in_pin, dio_bsp_pin_state_e in_state);

/*! \brief Read the current level of one pin.
 *
 * Reads the input register, so it reports the level actually present on the pin
 * for both inputs and push-pull outputs.
 *
 * \param[in]       in_pin: pin to read, a member of \ref dio_bsp_pin_e
 * \return          \ref DIO_BSP_PIN_STATE_HIGH or \ref DIO_BSP_PIN_STATE_LOW
 */
dio_bsp_pin_state_e dio_bsp_get_pin(dio_bsp_pin_e in_pin);

/*! \brief Invert the current level of one output pin.
 *
 * \param[in]       in_pin: pin to toggle, a member of \ref dio_bsp_pin_e
 */
void dio_bsp_toggle_pin(dio_bsp_pin_e in_pin);

#endif /* DIO_BSP_H */
