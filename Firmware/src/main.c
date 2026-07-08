/*
 * MicroPacControllerMan — Milestone 1: Toolchain Bring-Up
 *
 * Proves the build/flash/run chain on the STM32G431RB Nucleo-64 and provides the
 * base On-Target Test (OTT) infrastructure:
 *   - blinks the on-board LED LD2 (PA5) at ~1 Hz (nominal behaviour)
 *   - serves an OTT CLI on the ST-LINK VCP (LPUART1, PA2/PA3, 115200 8N1):
 *       `ott blinky`  -> drives PA5 and reads the pin back  -> OTT PASSED/FAILED
 *       `ott list`    -> lists available tests
 *
 * Register-level init (no HAL) against CMSIS. Default reset clock is HSI 16 MHz;
 * SystemInit() (system_stm32g4xx.c) runs from the startup file before main().
 */
#include "stm32g4xx.h"

#include "led.h"
#include "ott.h"
#include "uart.h"

#include <stdint.h>

static volatile uint32_t g_ms;

void SysTick_Handler(void) { g_ms++; }

int main(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U); /* 1 ms tick */
    led_init();
    uart_init();

    uart_write("\r\nMicroPacControllerMan M1 - toolchain bring-up OK\r\n");
    uart_write("OTT CLI ready. Try: 'ott list' or 'ott blinky'.\r\n");

    uint32_t last_toggle = 0;
    for (;;) {
        /* nominal ~1 Hz blink, non-blocking */
        if ((g_ms - last_toggle) >= 500U) {
            last_toggle = g_ms;
            led_toggle();
        }
        /* service the OTT command line */
        ott_cli_poll();
    }
}
