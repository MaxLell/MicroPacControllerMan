/*
 * Target implementation of the display port: LCD Mono Click, Sharp LS013B7DH03,
 * 128x128 1 bpp, over SPI with three plain digital control lines.
 *
 * The panel is write-only, so there is nothing to read back — a frame is pushed in
 * full, line by line, inside one chip-select window.
 *
 * COM polarity has to be inverted at least once a second while an unchanging image is
 * held or the liquid crystal degrades. The panel can take that from a software command
 * bit or from an external clock pin, selected by a jumper on the Click board;
 * display_service() drives both, so either jumper position works.
 */
#include "display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "dio_bsp.h"
#include "framebuffer.h"
#include "spi_bsp.h"

/* ==========================================================================
 * display - private
 * ========================================================================= */

/* LS013B7DH03 mode-byte bits, in LSB-first wire order (matches the SPI setup). */
#define DISPLAY_COMMAND_WRITE_LINE       (0x01U)
#define DISPLAY_COMMAND_VCOM             (0x02U)
#define DISPLAY_COMMAND_CLEAR_ALL        (0x04U)

/* Line addresses are 1-based on the wire. */
#define DISPLAY_FIRST_LINE_ADDRESS       (1U)

/* Every command and every line is closed with a don't-care byte. */
#define DISPLAY_PADDING_BYTE             (0x00U)

/* The panel wants ~6 us of setup and ~2 us of hold around chip-select.
 *
 * This is a spin count, not a time, so it scales inversely with the core clock. It
 * was tuned when the firmware ran at 16 MHz; the clock is now 170 MHz (DEC-004),
 * which shortened it by roughly 10x to an estimated 6-11 us — i.e. it now only
 * barely clears the 6 us setup requirement, and would silently violate it on any
 * further clock or compiler change. Widened so the margin is real again; the cost
 * is a few microseconds twice per command and twice per frame, which is noise next
 * to the ~2.3 kB a frame clocks out. Tracked as RF-006. */
#define DISPLAY_CHIP_SELECT_SETTLE_LOOPS (600U)

/* Holds the VCOM command bit itself, so it can be OR-ed into a header directly. */
static uint8_t g_vcom_state;

static void prv_set_chip_select(dio_bsp_pin_state_e in_state)
{
    /* Chip-select is ACTIVE-HIGH on this panel. */
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_CS, in_state);
}

static void prv_wait_chip_select_settle(void)
{
    for (volatile uint32_t loop = 0U; loop < DISPLAY_CHIP_SELECT_SETTLE_LOOPS; ++loop)
    {
    }
}

static uint8_t prv_toggle_vcom(void)
{
    g_vcom_state ^= DISPLAY_COMMAND_VCOM;

    return g_vcom_state;
}

/* Send a bare two-byte command frame, chip-select included. */
static void prv_send_command(uint8_t in_command)
{
    const uint8_t frame[] = {in_command, DISPLAY_PADDING_BYTE};

    prv_set_chip_select(DIO_BSP_PIN_STATE_HIGH);
    prv_wait_chip_select_settle();

    spi_bsp_write(frame, sizeof(frame));

    prv_wait_chip_select_settle();
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
}

static void prv_write_line(const framebuffer_t* const in_framebuffer, int16_t in_line)
{
    const uint8_t* const logical_bits = framebuffer_get_line(in_framebuffer, in_line);
    const uint8_t line_address = (uint8_t)(in_line + DISPLAY_FIRST_LINE_ADDRESS);
    const uint8_t padding = DISPLAY_PADDING_BYTE;
    uint8_t panel_bits[FRAMEBUFFER_BYTES_PER_LINE];

    /* The panel's bit sense is the inverse of the frame buffer's: a set bit means
     * white on the wire, ink in the buffer. Inverting here keeps the polarity a panel
     * detail instead of leaking it into everything that draws. */
    for (size_t index = 0U; index < sizeof(panel_bits); ++index)
    {
        panel_bits[index] = (uint8_t)~logical_bits[index];
    }

    spi_bsp_write(&line_address, sizeof(line_address));
    spi_bsp_write(panel_bits, sizeof(panel_bits));
    spi_bsp_write(&padding, sizeof(padding));
}

/* ==========================================================================
 * display - public
 * ========================================================================= */

void display_init(void)
{
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_EXTCOMIN, DIO_BSP_PIN_STATE_LOW);

    display_set_enabled(true);

    g_vcom_state = 0U;

    display_clear();
}

void display_present(const framebuffer_t* in_framebuffer)
{
    const uint8_t padding = DISPLAY_PADDING_BYTE;
    uint8_t header;

    ASSERT(in_framebuffer != NULL);

    header = (uint8_t)(DISPLAY_COMMAND_WRITE_LINE | prv_toggle_vcom());

    prv_set_chip_select(DIO_BSP_PIN_STATE_HIGH);
    prv_wait_chip_select_settle();

    spi_bsp_write(&header, sizeof(header));

    for (int16_t line = 0; line < FRAMEBUFFER_HEIGHT; ++line)
    {
        prv_write_line(in_framebuffer, line);
    }

    /* Closing dummy byte, which together with the last line's padding forms the
     * 16-bit trailer the panel expects. */
    spi_bsp_write(&padding, sizeof(padding));

    prv_wait_chip_select_settle();
    prv_set_chip_select(DIO_BSP_PIN_STATE_LOW);
}

void display_clear(void)
{
    prv_send_command((uint8_t)(DISPLAY_COMMAND_CLEAR_ALL | g_vcom_state));
}

void display_set_enabled(bool in_is_enabled)
{
    dio_bsp_set_pin(DIO_BSP_PIN_DISPLAY_DISP, in_is_enabled ? DIO_BSP_PIN_STATE_HIGH : DIO_BSP_PIN_STATE_LOW);
}

void display_service(void)
{
    /* Software inversion: a no-update command that only carries the toggled bit. */
    prv_send_command(prv_toggle_vcom());

    /* External inversion: pulse the dedicated clock pin as well, so the driver does
     * not care which mode the board's jumper selects. */
    dio_bsp_toggle_pin(DIO_BSP_PIN_DISPLAY_EXTCOMIN);
}
