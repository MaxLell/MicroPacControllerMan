/*
 * joystick.h
 *
 * The X-NUCLEO-GFX01M2's five-key joystick, debounced — a concrete instance of the
 * switch primitive, the way user_button is the instance for B1.
 *
 * All five keys are active low and the shield pulls them up itself, so no internal
 * pull is configured. The pin map was measured rather than taken from UM2750; see
 * [M2 Board Bring-Up §1](../../../Docu/Design/M2-Board-Bring-Up.md).
 */

#ifndef BSP_JOYSTICK_H
#define BSP_JOYSTICK_H

#include <stdbool.h>

#include "switch.h"

/* ==========================================================================
 * joystick - public types
 * ========================================================================= */

/*! \brief Delay from a settled contact to the key being reported, in milliseconds.
 *
 * The debounce window of the switch primitive, at the 1 ms rate #joystick_poll is
 * called at. It is the floor under the input latency, before anything is drawn — see
 * [M2 Board Bring-Up §5](../../../Docu/Design/M2-Board-Bring-Up.md). */
#define JOYSTICK_DEBOUNCE_MS (SWITCH_DEBOUNCE_SAMPLES)

typedef enum
{
    JOYSTICK_KEY_NORTH = 0,
    JOYSTICK_KEY_EAST,
    JOYSTICK_KEY_SOUTH,
    JOYSTICK_KEY_WEST,
    JOYSTICK_KEY_CENTER,
    JOYSTICK_KEY_COUNT
} joystick_key_e;

/* ==========================================================================
 * joystick - public API
 * ========================================================================= */

/*! \brief Initialize all five debounced keys.
 *
 * Clears the debounce history and reports every key as released.
 */
void joystick_init(void);

/*! \brief Update all five debounced keys from the 1 ms timer ISR.
 *
 * Must be called every 1 ms for the debounce timing to hold. Latches a press edge
 * per key for #joystick_take_press. Does nothing until #joystick_init has run.
 */
void joystick_poll(void);

/*! \brief Return the latest debounced state of one key.
 *
 * \param[in]       in_key: key to read, below \ref JOYSTICK_KEY_COUNT
 * \return          `true` while the key is held down
 */
bool joystick_is_pressed(joystick_key_e in_key);

/*! \brief Consume one latched press edge of one key.
 *
 * Returns `true` exactly once per press and clears the latch, so a held key is
 * reported a single time — one press, one step.
 *
 * \param[in]       in_key: key to read, below \ref JOYSTICK_KEY_COUNT
 * \return          `true` when that key has been pressed since the last call
 */
bool joystick_take_press(joystick_key_e in_key);

/*! \brief The key's name, for a test or log line that has to say which one moved.
 *
 * Keeps the name beside the pin it belongs to instead of in every caller.
 *
 * \param[in]       in_key: key to name, below \ref JOYSTICK_KEY_COUNT
 * \return          Static string, never `NULL`
 */
const char* joystick_get_key_name(joystick_key_e in_key);

#endif /* BSP_JOYSTICK_H */
