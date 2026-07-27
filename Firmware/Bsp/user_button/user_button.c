#include "user_button.h"

#include <stdbool.h>
#include <stdint.h>

#include "dio_bsp.h"
#include "switch.h"

/* ==========================================================================
 * user_button - private
 * ========================================================================= */

/* The NUCLEO-G431RB wires B1 ACTIVE-HIGH: idle low through the board's
 * pull-down, pressing drives PC13 to VDD (measured 3.3 V). CubeMX cannot
 * express that polarity, so it lives here. */
#define USER_BUTTON_IS_ACTIVE_LOW (false)

static volatile bool g_is_initialized = false;
static volatile bool g_is_pressed = false;
static volatile bool g_is_press_latched = false;
static switch_t g_user_button;

/* ==========================================================================
 * user_button - public
 * ========================================================================= */

void user_button_init(void)
{
    switch_init(&g_user_button, DIO_BSP_PIN_USER_BUTTON, USER_BUTTON_IS_ACTIVE_LOW);

    g_is_pressed = false;
    g_is_press_latched = false;
    g_is_initialized = true;
}

bool user_button_poll(void)
{
    bool is_pressed;

    if (!g_is_initialized)
    {
        return false;
    }

    is_pressed = switch_get_debounced_state(&g_user_button);

    if (is_pressed && !g_is_pressed)
    {
        g_is_press_latched = true;
    }

    g_is_pressed = is_pressed;

    return g_is_pressed;
}

bool user_button_is_pressed(void)
{
    bool is_pressed;
    const uint32_t primask = switch_enter_critical_section();

    is_pressed = g_is_pressed;

    switch_exit_critical_section(primask);

    return is_pressed;
}

bool user_button_take_press(void)
{
    bool is_press_latched;
    const uint32_t primask = switch_enter_critical_section();

    is_press_latched = g_is_press_latched;
    g_is_press_latched = false;

    switch_exit_critical_section(primask);

    return is_press_latched;
}
