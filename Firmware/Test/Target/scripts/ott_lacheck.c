#include "ott_lacheck.h"

#include "bsp_spi.h"
#include "button.h"
#include "systick.h"
#include "uart.h"

#include "main.h" /* DISPLAY_* pin macros + HAL_GPIO (from the CubeMX export) */

#include <string.h>

/*
 * All five signals are already configured by MX_GPIO_Init / MX_SPI1_Init (run in
 * main() before app_main()): CS/DISP/EXTCOMIN are push-pull GPIO outputs, SCK/MOSI
 * are SPI1 alternate-function. This probe drives them directly; it does not touch
 * the display driver, so the capture contains only the deliberate fingerprints.
 */

#define PULSE_HI_MS  (120U)  /* GPIO pulse: high time                    */
#define PULSE_LO_MS  (280U)  /* GPIO pulse: low time (400 ms period)     */
#define GROUP_GAP_MS (1200U) /* idle between pin groups                  */
#define SYNC_GAP_MS  (2500U) /* long idle = end-of-sweep marker          */

#define SPI_BURSTS   (6U)    /* spaced SPI transfers per sweep           */
#define BURST_LEN    (256U)  /* bytes per burst (~3 ms at ~0.66 MHz)     */
#define BURST_GAP_MS (300U)  /* idle between SPI bursts                  */

#define LACHECK_MAX_MS (180000U) /* safety cap so the OTT always ends    */

/* delay `ms`, polling the button; returns 1 if pressed (finish early). */
static int prv_wait(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        if (button_pressed()) {
            return 1;
        }
        delay_ms(5);
    }
    return 0;
}

/* Emit `count` clean high pulses on one GPIO. Returns 1 if the button was
 * pressed (finish early), else 0. */
static int prv_pulses(GPIO_TypeDef* port, uint16_t pin, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        if (prv_wait(PULSE_HI_MS)) {
            HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
            return 1;
        }
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        if (prv_wait(PULSE_LO_MS)) {
            return 1;
        }
    }
    return 0;
}

/* Alternating 0x00 / 0xFF SPI bursts: SCK clocks in every burst, MOSI is flat
 * within a burst and HIGH only on the 0xFF ones. Returns 1 if button pressed. */
static int prv_spi_bursts(void)
{
    static uint8_t buf[BURST_LEN];
    for (unsigned i = 0; i < SPI_BURSTS; i++) {
        memset(buf, (i & 1U) ? 0xFF : 0x00, sizeof(buf));
        spi_write(buf, sizeof(buf));
        if (prv_wait(BURST_GAP_MS)) {
            return 1;
        }
    }
    return 0;
}

int ott_lacheck_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0;
    return 1;
}

int ott_lacheck_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;
    (void)reason;
    (void)reason_size;

    button_init();

    /* Known idle state: all GPIO outputs low; SPI lines idle (SCK low, mode 0). */
    HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISPLAY_DISP_GPIO_Port, DISPLAY_DISP_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISPLAY_EXTCOMIN_GPIO_Port, DISPLAY_EXTCOMIN_Pin, GPIO_PIN_RESET);

    uart_write("\r\n=== Logic-analyzer wiring check (lacheck) ===\r\n");
    uart_write("Hook the probes to these NUCLEO pins, then capture a full sweep:\r\n");
    uart_write("  CS=PB6, DISP=PA6, EXTCOMIN=PB10, SCK=PA5, MOSI=PA7 (+ GND).\r\n");
    uart_write("Fingerprint per sweep (all lines start LOW):\r\n");
    uart_write("  CS(PB6)      : 2 slow pulses\r\n");
    uart_write("  DISP(PA6)    : 3 slow pulses\r\n");
    uart_write("  EXTCOMIN(PB10): 4 slow pulses\r\n");
    uart_write("  SCK(PA5)     : fast clock in every SPI burst\r\n");
    uart_write("  MOSI(PA7)    : HIGH only on the 0xFF bursts, else LOW\r\n");
    uart_write("A 2.5 s all-low gap marks the end of each sweep. B1 finishes.\r\n\r\n");

    uint32_t start = millis();
    unsigned sweep = 0;
    while ((millis() - start) < LACHECK_MAX_MS) {
        sweep++;
        uart_write("-- sweep start --\r\n");

        uart_write("  CS(PB6): 2 pulses\r\n");
        if (prv_pulses(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, 2)) {
            break;
        }
        if (prv_wait(GROUP_GAP_MS)) {
            break;
        }

        uart_write("  DISP(PA6): 3 pulses\r\n");
        if (prv_pulses(DISPLAY_DISP_GPIO_Port, DISPLAY_DISP_Pin, 3)) {
            break;
        }
        if (prv_wait(GROUP_GAP_MS)) {
            break;
        }

        uart_write("  EXTCOMIN(PB10): 4 pulses\r\n");
        if (prv_pulses(DISPLAY_EXTCOMIN_GPIO_Port, DISPLAY_EXTCOMIN_Pin, 4)) {
            break;
        }
        if (prv_wait(GROUP_GAP_MS)) {
            break;
        }

        uart_write("  SCK/MOSI: 6 SPI bursts (0x00/0xFF alternating)\r\n");
        if (prv_spi_bursts()) {
            break;
        }

        if (prv_wait(SYNC_GAP_MS)) {
            break;
        }
    }

    /* Leave everything low on exit. */
    HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port, DISPLAY_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISPLAY_DISP_GPIO_Port, DISPLAY_DISP_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISPLAY_EXTCOMIN_GPIO_Port, DISPLAY_EXTCOMIN_Pin, GPIO_PIN_RESET);

    uart_write("\r\nlacheck finished.\r\n");
    return 1;
}
