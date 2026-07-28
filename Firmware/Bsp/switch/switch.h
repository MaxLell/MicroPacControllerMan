/*
 * switch.h
 *
 * Shared debounced GPIO input primitive for user switch modules.
 */

#ifndef BSP_SWITCH_H
#define BSP_SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#include "dio_bsp.h"

/* ==========================================================================
 * switch - public types
 * ========================================================================= */

/*! \brief Number of consecutive samples that must agree before a state is
 *         reported. At the intended 1 ms sample rate this is the debounce time
 *         in milliseconds. Bounded by the width of switch_t::history. */
#define SWITCH_DEBOUNCE_SAMPLES (32U)

typedef struct
{
    dio_bsp_pin_e pin; /*!< Logical pin the switch is wired to      */
    bool active_low;   /*!< Pin reads LOW while the switch is active */
} switch_gpio_t;

typedef struct
{
    switch_gpio_t gpio; /*!< GPIO configuration metadata             */
    uint32_t history;   /*!< Shift register of the last samples      */
    bool is_active;     /*!< Latest debounced state                  */
} switch_t;

/* ==========================================================================
 * switch - public API
 * ========================================================================= */

/*! \brief Initialize one debounced GPIO switch input instance.
 *
 * Applies GPIO configuration metadata and resets debounce history/state to
 * inactive.
 *
 * \param[out]      inout_switch: instance to initialize
 * \param[in]       in_pin: logical pin, a member of \ref dio_bsp_pin_e
 * \param[in]       in_active_low: `true` when the pin reads LOW while active
 */
void switch_init(switch_t* inout_switch, dio_bsp_pin_e in_pin, bool in_active_low);

/*! \brief Sample and debounce one GPIO switch input.
 *
 * Shifts one fresh sample into the internal debounce history and returns the
 * currently debounced active/inactive state. The state only changes after
 * #SWITCH_DEBOUNCE_SAMPLES consecutive samples agree.
 *
 * \param[in,out]   inout_switch: instance to sample
 * \return          `true` while the switch is debounced-active
 */
bool switch_get_debounced_state(switch_t* inout_switch);

/*! \brief Enter a critical section by disabling IRQs.
 *
 * Returns the previous PRIMASK value which must be passed to
 * #switch_exit_critical_section to restore the prior IRQ state.
 *
 * \return          PRIMASK value to hand back when leaving the critical section
 */
uint32_t switch_enter_critical_section(void);

/*! \brief Exit a critical section previously entered.
 *
 * Restores the PRIMASK value returned by #switch_enter_critical_section.
 *
 * \param[in]       in_primask: value returned by #switch_enter_critical_section
 */
void switch_exit_critical_section(uint32_t in_primask);

#endif /* BSP_SWITCH_H */
