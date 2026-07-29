#include "ott_blinky.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "delay.h"
#include "dio_bsp.h"

/* ==========================================================================
 * ott_blinky - private
 * ========================================================================= */

/* The pin settles in nanoseconds; this exists so the read is a deliberate second
 * bus access rather than something that happens to work. */
#define OTT_BLINKY_SETTLE_MS (1U)

/* Visible confirmation on success: five on/off cycles, one second in total. */
#define OTT_BLINKY_VISIBLE_CYCLES (5U)
#define OTT_BLINKY_VISIBLE_HALF_PERIOD_MS (100U)

static const char* prv_level_name(dio_bsp_pin_state_e in_state)
{
    return (in_state == DIO_BSP_PIN_STATE_HIGH) ? "HIGH" : "LOW";
}

/* Drives the LED pin and reports whether the pin actually reached that level. */
static bool prv_drive_and_verify(dio_bsp_pin_state_e in_expected, char* out_reason,
                                size_t in_reason_size)
{
    dio_bsp_pin_state_e actual;
    bool is_correct;

    dio_bsp_set_pin(DIO_BSP_PIN_LED_GREEN, in_expected);
    delay_ms(OTT_BLINKY_SETTLE_MS);

    actual = dio_bsp_get_pin(DIO_BSP_PIN_LED_GREEN);
    is_correct = (actual == in_expected);

    cli_print("LED driven %s, read back %s", prv_level_name(in_expected), prv_level_name(actual));

    if (!is_correct)
    {
        (void)snprintf(out_reason, in_reason_size, "drove PA5 %s but read back %s",
                       prv_level_name(in_expected), prv_level_name(actual));
    }

    return is_correct;
}

/* Toggles the LED pin and reports whether it landed on the opposite level. */
static bool prv_toggle_and_verify(dio_bsp_pin_state_e in_expected, char* out_reason,
                                 size_t in_reason_size)
{
    dio_bsp_pin_state_e actual;
    bool is_correct;

    dio_bsp_toggle_pin(DIO_BSP_PIN_LED_GREEN);
    delay_ms(OTT_BLINKY_SETTLE_MS);

    actual = dio_bsp_get_pin(DIO_BSP_PIN_LED_GREEN);
    is_correct = (actual == in_expected);

    cli_print("LED toggled, expected %s, read back %s", prv_level_name(in_expected),
              prv_level_name(actual));

    if (!is_correct)
    {
        (void)snprintf(out_reason, in_reason_size, "toggled PA5 expecting %s but read back %s",
                       prv_level_name(in_expected), prv_level_name(actual));
    }

    return is_correct;
}

/* Leaves the LED off. */
static void prv_blink_visibly(void)
{
    cli_print("LED verified — blinking %u times so you can see it", OTT_BLINKY_VISIBLE_CYCLES);

    for (uint32_t cycle = 0U; cycle < OTT_BLINKY_VISIBLE_CYCLES; ++cycle)
    {
        dio_bsp_set_pin(DIO_BSP_PIN_LED_GREEN, DIO_BSP_PIN_STATE_HIGH);
        delay_ms(OTT_BLINKY_VISIBLE_HALF_PERIOD_MS);

        dio_bsp_set_pin(DIO_BSP_PIN_LED_GREEN, DIO_BSP_PIN_STATE_LOW);
        delay_ms(OTT_BLINKY_VISIBLE_HALF_PERIOD_MS);
    }
}

/* ==========================================================================
 * ott_blinky - public
 * ========================================================================= */

bool ott_blinky_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_passed;

    (void)in_parameter;

    cli_print("Blinky test: driving LD2 (PA5) and reading the pin back.");

    /* Drive both levels, then reach each of them once more by toggling — a pin stuck
     * at one level, and a toggle that does not invert, are different faults. */
    has_passed = prv_drive_and_verify(DIO_BSP_PIN_STATE_HIGH, out_reason, in_reason_size)
                 && prv_drive_and_verify(DIO_BSP_PIN_STATE_LOW, out_reason, in_reason_size)
                 && prv_toggle_and_verify(DIO_BSP_PIN_STATE_HIGH, out_reason, in_reason_size)
                 && prv_toggle_and_verify(DIO_BSP_PIN_STATE_LOW, out_reason, in_reason_size);

    if (has_passed)
    {
        prv_blink_visibly();
    }
    else
    {
        dio_bsp_set_pin(DIO_BSP_PIN_LED_GREEN, DIO_BSP_PIN_STATE_LOW);
    }

    return has_passed;
}
