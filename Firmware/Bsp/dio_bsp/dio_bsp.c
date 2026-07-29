#include "dio_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "main.h"

/* ==========================================================================
 * dio_bsp - private
 * ========================================================================= */

typedef struct
{
    GPIO_TypeDef* port;
    uint16_t pin_mask;
} dio_bsp_pin_map_t;

/* The pin macros come from the STM32CubeMX export (Core/Inc/main.h), so the
 * physical mapping stays in the .ioc and this table only names it. */
static const dio_bsp_pin_map_t g_pin_map[DIO_BSP_PIN_LAST] = {
    [DIO_BSP_PIN_NONE] = {NULL, 0U},
    [DIO_BSP_PIN_USER_BUTTON] = {USER_BUTTON_GPIO_Port, USER_BUTTON_Pin},
    [DIO_BSP_PIN_LED_GREEN] = {LED_GREEN_GPIO_Port, LED_GREEN_Pin},
};

static bool g_is_initialized = false;

static const dio_bsp_pin_map_t* prv_get_pin_map(dio_bsp_pin_e in_pin)
{
    ASSERT(g_is_initialized);
    ASSERT(in_pin > DIO_BSP_PIN_NONE);
    ASSERT(in_pin < DIO_BSP_PIN_LAST);
    ASSERT(g_pin_map[in_pin].port != NULL);

    return &g_pin_map[in_pin];
}

/* ==========================================================================
 * dio_bsp - public
 * ========================================================================= */

void dio_bsp_init(void)
{
    ASSERT(false == g_is_initialized);

    g_is_initialized = true;
}

void dio_bsp_set_pin(dio_bsp_pin_e in_pin, dio_bsp_pin_state_e in_state)
{
    const dio_bsp_pin_map_t* const pin_map = prv_get_pin_map(in_pin);

    HAL_GPIO_WritePin(pin_map->port, pin_map->pin_mask,
                      (in_state == DIO_BSP_PIN_STATE_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

dio_bsp_pin_state_e dio_bsp_get_pin(dio_bsp_pin_e in_pin)
{
    const dio_bsp_pin_map_t* const pin_map = prv_get_pin_map(in_pin);

    return (HAL_GPIO_ReadPin(pin_map->port, pin_map->pin_mask) == GPIO_PIN_SET)
               ? DIO_BSP_PIN_STATE_HIGH
               : DIO_BSP_PIN_STATE_LOW;
}

void dio_bsp_toggle_pin(dio_bsp_pin_e in_pin)
{
    const dio_bsp_pin_map_t* const pin_map = prv_get_pin_map(in_pin);

    HAL_GPIO_TogglePin(pin_map->port, pin_map->pin_mask);
}
