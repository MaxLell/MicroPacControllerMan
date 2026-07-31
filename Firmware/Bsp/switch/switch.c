#include "switch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "dio_bsp.h"
#include "main.h"

/* ==========================================================================
 * switch - private
 * ========================================================================= */

#define SWITCH_HISTORY_STABLE_ACTIVE   (0xFFFFFFFFU)
#define SWITCH_HISTORY_STABLE_INACTIVE (0x00000000U)

#define SWITCH_BITS_PER_BYTE           (8U)

_Static_assert(SWITCH_DEBOUNCE_SAMPLES == (sizeof(uint32_t) * SWITCH_BITS_PER_BYTE),
               "the debounce window must match the width of switch_t::history");

static bool prv_read_gpio_level(const switch_t* const in_switch)
{
    ASSERT(in_switch != NULL);

    const bool pin_is_high = (dio_bsp_get_pin(in_switch->gpio.pin) == DIO_BSP_PIN_STATE_HIGH);

    if (in_switch->gpio.active_low)
    {
        return !pin_is_high;
    }

    return pin_is_high;
}

/* ==========================================================================
 * switch - public
 * ========================================================================= */

void switch_init(switch_t* inout_switch, dio_bsp_pin_e in_pin, bool in_active_low)
{
    ASSERT(inout_switch != NULL);
    ASSERT(in_pin > DIO_BSP_PIN_NONE);
    ASSERT(in_pin < DIO_BSP_PIN_LAST);

    inout_switch->gpio.pin = in_pin;
    inout_switch->gpio.active_low = in_active_low;

    inout_switch->history = SWITCH_HISTORY_STABLE_INACTIVE;
    inout_switch->is_active = false;
}

bool switch_get_debounced_state(switch_t* inout_switch)
{
    ASSERT(inout_switch != NULL);

    inout_switch->history = (inout_switch->history << 1U) | (uint32_t)prv_read_gpio_level(inout_switch);

    if (inout_switch->history == SWITCH_HISTORY_STABLE_ACTIVE)
    {
        inout_switch->is_active = true;
    }
    else if (inout_switch->history == SWITCH_HISTORY_STABLE_INACTIVE)
    {
        inout_switch->is_active = false;
    }
    else
    {
        /* Still bouncing: hold the last debounced state. */
    }

    return inout_switch->is_active;
}

uint32_t switch_enter_critical_section(void)
{
    const uint32_t primask = __get_PRIMASK();

    __disable_irq();

    return primask;
}

void switch_exit_critical_section(uint32_t in_primask)
{
    __set_PRIMASK(in_primask);
}
