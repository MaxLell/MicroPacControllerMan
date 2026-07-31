#include "joystick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "dio_bsp.h"
#include "switch.h"

/* ==========================================================================
 * joystick - private
 * ========================================================================= */

#define JOYSTICK_IS_ACTIVE_LOW (true)

typedef struct
{
    dio_bsp_pin_e pin;
    const char* name;
} joystick_key_map_t;

/* Indexed by joystick_key_e, so the order here is the order of the enum. */
static const joystick_key_map_t g_key_map[JOYSTICK_KEY_COUNT] = {
    {DIO_BSP_PIN_JOYSTICK_NORTH, "NORTH"},   {DIO_BSP_PIN_JOYSTICK_EAST, "EAST"},
    {DIO_BSP_PIN_JOYSTICK_SOUTH, "SOUTH"},   {DIO_BSP_PIN_JOYSTICK_WEST, "WEST"},
    {DIO_BSP_PIN_JOYSTICK_CENTER, "CENTER"},
};

static volatile bool g_is_initialized = false;
static volatile bool g_is_pressed[JOYSTICK_KEY_COUNT] = {false};
static volatile bool g_is_press_latched[JOYSTICK_KEY_COUNT] = {false};
static switch_t g_keys[JOYSTICK_KEY_COUNT];

/* ==========================================================================
 * joystick - public
 * ========================================================================= */

void joystick_init(void)
{
    for (size_t index = 0U; index < (size_t)JOYSTICK_KEY_COUNT; ++index)
    {
        switch_init(&g_keys[index], g_key_map[index].pin, JOYSTICK_IS_ACTIVE_LOW);

        g_is_pressed[index] = false;
        g_is_press_latched[index] = false;
    }

    g_is_initialized = true;
}

void joystick_poll(void)
{
    if (!g_is_initialized)
    {
        return;
    }

    for (size_t index = 0U; index < (size_t)JOYSTICK_KEY_COUNT; ++index)
    {
        const bool is_pressed = switch_get_debounced_state(&g_keys[index]);

        if (is_pressed && !g_is_pressed[index])
        {
            g_is_press_latched[index] = true;
        }

        g_is_pressed[index] = is_pressed;
    }
}

bool joystick_is_pressed(joystick_key_e in_key)
{
    bool is_pressed;
    uint32_t primask;

    ASSERT(in_key < JOYSTICK_KEY_COUNT);

    primask = switch_enter_critical_section();

    is_pressed = g_is_pressed[in_key];

    switch_exit_critical_section(primask);

    return is_pressed;
}

bool joystick_take_press(joystick_key_e in_key)
{
    bool is_press_latched;
    uint32_t primask;

    ASSERT(in_key < JOYSTICK_KEY_COUNT);

    primask = switch_enter_critical_section();

    is_press_latched = g_is_press_latched[in_key];
    g_is_press_latched[in_key] = false;

    switch_exit_critical_section(primask);

    return is_press_latched;
}

const char* joystick_get_key_name(joystick_key_e in_key)
{
    ASSERT(in_key < JOYSTICK_KEY_COUNT);

    return g_key_map[in_key].name;
}
