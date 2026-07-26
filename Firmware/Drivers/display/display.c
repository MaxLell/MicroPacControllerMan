#include "display.h"

#include "bsp_spi.h"

#include "main.h" /* DISPLAY_CS/DISP/EXTCOMIN pin macros + HAL (from the CubeMX export) */

#include <string.h>

/* mikroBUS slot-1 control pins, configured as GPIO outputs by MX_GPIO_Init.
 * Verified against the LCD Mono Click schematic v100 (R-001):
 *   CS       = PB6  (mikroBUS CS,   DISPLAY_CS)       SCS, active HIGH
 *   DISP     = PA6  (mikroBUS MISO, DISPLAY_DISP)     panel on/off, HIGH = on.
 *                    The LS013B7DH03 is write-only, so the Click reuses the idle
 *                    MISO line for DISP; PA6 is a plain GPIO output, not SPI MISO.
 *   EXTCOMIN = PB10 (mikroBUS PWM,  DISPLAY_EXTCOMIN) external VCOM clock.
 *
 * COM inversion is selected by the Click's JP1 (MODE SEL) jumper; the default
 * (LEFT / EXTMODE=0) = software inversion via the M1 (CMD_VCOM) bit, which this
 * driver sets on every flush. EXTCOMIN is a don't-care in that mode; we also
 * toggle it in display_vcom_tick() so a JP1=RIGHT (external VCOM) board works too.
 */

/* LS013B7DH03 mode-byte bits, in LSB-first wire order (matches the SPI setup). */
#define CMD_WRITE 0x01U /* M0: data update / write line(s) */
#define CMD_VCOM  0x02U /* M1: VCOM state (software COM inversion) */
#define CMD_CLEAR 0x04U /* M2: clear all to white */

/* Framebuffer in panel-native bits: 1 = white, 0 = black. Bit 0 (LSB) is the
 * left-most pixel of the byte, matching the LSB-first transfer. */
static uint8_t g_fb[DISPLAY_HEIGHT][DISPLAY_WIDTH / 8];
static uint8_t g_vcom; /* toggles each flush/tick to keep COM inversion alive */

static void prv_cs(int high)
{
    HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* A few microseconds of settle around CS (LS013B7DH03 tsSCS/thSCS >= ~6/2 us). */
static void prv_cs_settle(void)
{
    for (volatile int i = 0; i < 200; i++) {
    }
}

void display_on(int on)
{
    HAL_GPIO_WritePin(DISPLAY_DISP_GPIO_Port, DISPLAY_DISP_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void display_init(void)
{
    spi_init();

    /* CS/DISP/EXTCOMIN are already GPIO outputs (MX_GPIO_Init); just set the
     * working start state: CS + EXTCOMIN low, DISP high (panel on). */
    prv_cs(0);
    HAL_GPIO_WritePin(DISPLAY_EXTCOMIN_GPIO_Port, DISPLAY_EXTCOMIN_Pin, GPIO_PIN_RESET);
    display_on(1);

    g_vcom = 0;
    display_all_clear();
}

void display_clear(void) { memset(g_fb, 0xFF, sizeof(g_fb)); }

void display_pixel(int x, int y, int color)
{
    if ((unsigned)x >= DISPLAY_WIDTH || (unsigned)y >= DISPLAY_HEIGHT) {
        return;
    }
    uint8_t mask = (uint8_t)(1U << (x & 7));
    if (color == DISPLAY_BLACK) {
        g_fb[y][x >> 3] &= (uint8_t)~mask; /* black = clear the bit */
    } else {
        g_fb[y][x >> 3] |= mask; /* white = set the bit */
    }
}

void display_flush(void)
{
    g_vcom ^= 1U;

    prv_cs(1);
    prv_cs_settle();

    uint8_t hdr = CMD_WRITE | (g_vcom ? CMD_VCOM : 0U);
    spi_write(&hdr, 1);

    for (int line = 0; line < DISPLAY_HEIGHT; line++) {
        uint8_t addr = (uint8_t)(line + 1); /* 1-based line address, LSB-first */
        uint8_t trailer = 0x00U;
        spi_write(&addr, 1);
        spi_write(g_fb[line], DISPLAY_WIDTH / 8);
        spi_write(&trailer, 1); /* per-line data-transfer/dummy byte */
    }
    uint8_t final_dummy = 0x00U; /* closing 16-bit dummy (with last line's trailer) */
    spi_write(&final_dummy, 1);

    prv_cs_settle();
    prv_cs(0);
}

void display_all_clear(void)
{
    display_clear();

    uint8_t hdr = CMD_CLEAR | (g_vcom ? CMD_VCOM : 0U);
    uint8_t dummy = 0x00U;
    prv_cs(1);
    prv_cs_settle();
    spi_write(&hdr, 1);
    spi_write(&dummy, 1);
    prv_cs_settle();
    prv_cs(0);
}

void display_vcom_tick(void)
{
    /* Software VCOM: a no-update command carrying the toggled VCOM bit. */
    g_vcom ^= 1U;
    uint8_t hdr = (g_vcom ? CMD_VCOM : 0U);
    uint8_t dummy = 0x00U;
    prv_cs(1);
    prv_cs_settle();
    spi_write(&hdr, 1);
    spi_write(&dummy, 1);
    prv_cs_settle();
    prv_cs(0);

    /* External VCOM: also pulse EXTCOMIN, so either EXTMODE jumper setting works. */
    HAL_GPIO_TogglePin(DISPLAY_EXTCOMIN_GPIO_Port, DISPLAY_EXTCOMIN_Pin);
}
