/*
 * user_button.h
 *
 * Debounced on-board user button B1 — a concrete instance of the switch
 * primitive.
 */

#ifndef BSP_USER_BUTTON_H
#define BSP_USER_BUTTON_H

#include <stdbool.h>

/* ==========================================================================
 * user_button - public API
 * ========================================================================= */

/*! \brief Initialize the debounced on-board user button module.
 *
 * Clears the debounce history and resets the exported state to released.
 */
void user_button_init(void);

/*! \brief Update the debounced button state from the 1 ms timer ISR.
 *
 * Must be called every 1 ms (e.g. from a 1 kHz timer ISR) for the debounce
 * timing to be correct. Latches a press edge for #user_button_take_press.
 *
 * Returns `false` until the module has been initialized via #user_button_init.
 *
 * \return          `true` while the button is debounced-pressed
 */
bool user_button_poll(void);

/*! \brief Return the latest debounced button state.
 *
 * This accessor returns the last state produced by #user_button_poll.
 *
 * \return          `true` while the button is held down
 */
bool user_button_is_pressed(void);

/*! \brief Consume one latched press edge.
 *
 * Returns `true` exactly once per completed press and clears the latch, so a
 * button that is held down is reported a single time. Lets a caller wait for an
 * operator confirmation without tracking edges itself.
 *
 * \return          `true` when a press has happened since the last call
 */
bool user_button_take_press(void);

#endif /* BSP_USER_BUTTON_H */
