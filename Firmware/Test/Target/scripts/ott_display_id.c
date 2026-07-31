#include "ott_display_id.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "delay.h"
#include "dio_bsp.h"
#include "spi_bsp.h"

/* ==========================================================================
 * ott_display_id - private
 * ========================================================================= */

/* ST7789V reset timing, generous: RESX must be low for at least 10 us, and the
 * controller needs up to 120 ms afterwards before it accepts commands. */
#define OTT_DISPLAY_ID_RESET_LOW_MS    (20U)
#define OTT_DISPLAY_ID_RESET_SETTLE_MS (150U)

/* Identification registers. RDDID returns a dummy byte followed by three identity
 * bytes; RDID1..3 each return a dummy plus one byte. Four bytes are read
 * throughout so the dummy is visible rather than silently consumed. */
#define OTT_DISPLAY_ID_COMMAND_RDDID   (0x04U)
#define OTT_DISPLAY_ID_COMMAND_RDID1   (0xDAU)
#define OTT_DISPLAY_ID_COMMAND_RDID2   (0xDBU)
#define OTT_DISPLAY_ID_COMMAND_RDID3   (0xDCU)

#define OTT_DISPLAY_ID_READ_LENGTH     (4U)

typedef struct
{
    uint8_t command;
    const char* name;
} ott_display_id_register_t;

static const ott_display_id_register_t g_registers[] = {
    {OTT_DISPLAY_ID_COMMAND_RDDID, "RDDID"},
    {OTT_DISPLAY_ID_COMMAND_RDID1, "RDID1"},
    {OTT_DISPLAY_ID_COMMAND_RDID2, "RDID2"},
    {OTT_DISPLAY_ID_COMMAND_RDID3, "RDID3"},
};

#define OTT_DISPLAY_ID_REGISTER_COUNT (sizeof(g_registers) / sizeof(g_registers[0]))

/* Chip-select is driven by hand because its polarity is what this test determines.
 * UM2750 claims active high; the ST7789V datasheet says CSX is active low, and the
 * manual repeats its claim for a flash chip whose pin is named CS#, so the manual is
 * not evidence. */
static void prv_select(bool in_is_active_high, bool in_is_selected)
{
    const bool drive_high = (in_is_active_high == in_is_selected);

    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_CS, drive_high ? DIO_BSP_PIN_STATE_HIGH : DIO_BSP_PIN_STATE_LOW);
}

static void prv_reset_controller(bool in_is_active_high)
{
    prv_select(in_is_active_high, false);

    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_RESET, DIO_BSP_PIN_STATE_LOW);
    delay_ms(OTT_DISPLAY_ID_RESET_LOW_MS);
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_RESET, DIO_BSP_PIN_STATE_HIGH);
    delay_ms(OTT_DISPLAY_ID_RESET_SETTLE_MS);
}

/* Sends one command with DCX low, then clocks out zeros to shift the reply in. */
static void prv_read_register(bool in_is_active_high, uint8_t in_command, uint8_t* out_data)
{
    const uint8_t zeros[OTT_DISPLAY_ID_READ_LENGTH] = {0U};

    prv_select(in_is_active_high, true);

    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DCX, DIO_BSP_PIN_STATE_LOW);
    spi_bsp_write(&in_command, sizeof(in_command));

    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DCX, DIO_BSP_PIN_STATE_HIGH);
    spi_bsp_transfer(zeros, out_data, OTT_DISPLAY_ID_READ_LENGTH);

    prv_select(in_is_active_high, false);
}

/* A bus with nothing answering reads as all zeroes or all ones. Anything else means
 * the controller drove the line. */
static bool prv_is_plausible(const uint8_t* in_data, size_t in_length)
{
    bool is_all_zero = true;
    bool is_all_ones = true;

    for (size_t index = 0U; index < in_length; ++index)
    {
        if (in_data[index] != 0x00U)
        {
            is_all_zero = false;
        }

        if (in_data[index] != 0xFFU)
        {
            is_all_ones = false;
        }
    }

    return !(is_all_zero || is_all_ones);
}

/* Reads every identification register at one polarity and reports what came back. */
static bool prv_probe_polarity(bool in_is_active_high)
{
    bool has_answered = false;

    cli_print("  --- chip select active %s ---", in_is_active_high ? "HIGH" : "LOW");

    prv_reset_controller(in_is_active_high);

    for (size_t index = 0U; index < OTT_DISPLAY_ID_REGISTER_COUNT; ++index)
    {
        uint8_t data[OTT_DISPLAY_ID_READ_LENGTH] = {0U};

        prv_read_register(in_is_active_high, g_registers[index].command, data);

        cli_print("  %s (0x%02X) -> %02X %02X %02X %02X", g_registers[index].name, (unsigned)g_registers[index].command,
                  (unsigned)data[0], (unsigned)data[1], (unsigned)data[2], (unsigned)data[3]);

        if (prv_is_plausible(data, sizeof(data)))
        {
            has_answered = true;
        }
    }

    return has_answered;
}

/* ==========================================================================
 * ott_display_id - public
 * ========================================================================= */

bool ott_display_id_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_answered_low;
    bool has_answered_high;
    bool has_passed;

    (void)in_parameter;

    cli_print("Display ID test: resetting the controller and reading its ID registers.");
    cli_print("Nothing is drawn — this only asks whether the display answers, and how.");

    has_answered_low = prv_probe_polarity(false);
    has_answered_high = prv_probe_polarity(true);

    has_passed = has_answered_low || has_answered_high;

    if (has_passed)
    {
        cli_print("Display answered with chip select active %s.",
                  has_answered_low ? (has_answered_high ? "LOW and HIGH" : "LOW") : "HIGH");
    }
    else
    {
        (void)snprintf(out_reason, in_reason_size, "no answer at either CS polarity — check SPI pins, DCX, RESET");
    }

    /* Leave the panel deselected and out of reset. */
    prv_select(false, false);
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_RESET, DIO_BSP_PIN_STATE_HIGH);

    return has_passed;
}
