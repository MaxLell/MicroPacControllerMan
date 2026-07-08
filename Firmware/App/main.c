/*
 * MicroPacControllerMan — firmware entry point.
 *
 * Boots any pending On-Target Test (OTT), prints a boot banner (VT-INT-002),
 * then runs the nominal super-loop: ~1 Hz LD2 blink + OTT CLI polling on the
 * ST-LINK VCP (LPUART1, PA2/PA3, 115200 8N1).
 *
 * M1 brought up the toolchain, blinky and the OTT reset framework. M2 (Board
 * Bring-Up) adds the display/touchpad drivers and their OTT scenarios (`ott`
 * lists them). Register-level init (no HAL) against CMSIS; default reset clock
 * HSI 16 MHz.
 */
#include "stm32g4xx.h"

#include "led.h"
#include "ott.h"
#include "systick.h"
#include "uart.h"

int main(void)
{
    systick_init();
    uart_init();

    ott_execute_pending(); /* if an `ott` command scheduled a test + reset, run it now */

    /* led_init() runs AFTER the OTT path so nominal blink is restored even after a
     * display/touchdot OTT reconfigured PA5 (shared LD2/SPI1-SCK) as an AF pin. */
    led_init();

    /* Boot banner — a known, readable line for the harness (VT-INT-002). */
    uart_write("\r\nMicroPacControllerMan booted (M2 board bring-up). Type 'ott' for tests.\r\n");

    ott_init(); /* set up the EmbeddedCli-based OTT console */

    uint32_t last_toggle = 0;
    for (;;) {
        /* nominal ~1 Hz blink, non-blocking */
        if ((millis() - last_toggle) >= 500U) {
            last_toggle = millis();
            led_toggle();
        }
        /* service the OTT command line */
        ott_poll();
    }
}
