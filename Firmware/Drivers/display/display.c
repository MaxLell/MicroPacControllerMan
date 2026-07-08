#include "display.h"

#include "spi.h"

#include "stm32g4xx.h"

#include <string.h>

/* mikroBUS slot-1 control pins (see display.h / R-001). */
#define CS_PIN       6U  /* PB6  */
#define DISP_PIN     6U  /* PA6  */
#define EXTCOMIN_PIN 10U /* PB10 */

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
    if (high) {
        GPIOB->BSRR = (1U << CS_PIN);
    } else {
        GPIOB->BSRR = (1U << (CS_PIN + 16U));
    }
}

/* A few microseconds of settle around CS (LS013B7DH03 tsSCS/thSCS >= ~6/2 us). */
static void prv_cs_settle(void)
{
    for (volatile int i = 0; i < 200; i++) {
    }
}

void display_on(int on)
{
    if (on) {
        GPIOA->BSRR = (1U << DISP_PIN);
    } else {
        GPIOA->BSRR = (1U << (DISP_PIN + 16U));
    }
}

void display_init(void)
{
    spi_init();

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    /* CS (PB6) and EXTCOMIN (PB10) as outputs, start low. */
    GPIOB->MODER &= ~(GPIO_MODER_MODE6_Msk | GPIO_MODER_MODE10_Msk);
    GPIOB->MODER |= (0x1U << GPIO_MODER_MODE6_Pos) | (0x1U << GPIO_MODER_MODE10_Pos);
    GPIOB->BSRR = (1U << (CS_PIN + 16U)) | (1U << (EXTCOMIN_PIN + 16U));

    /* DISP (PA6) as output, start high (panel on). */
    GPIOA->MODER &= ~GPIO_MODER_MODE6_Msk;
    GPIOA->MODER |= (0x1U << GPIO_MODER_MODE6_Pos);
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
    GPIOB->ODR ^= (1U << EXTCOMIN_PIN);
}
