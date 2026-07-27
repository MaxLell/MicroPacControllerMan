#include "ott_button.h"

#include "button.h"
#include "systick.h"
#include "uart.h"

#include <stdio.h>
#include <string.h>

/* Diagnostic cap so the board still returns to nominal mode if nothing happens. */
#define BUTTON_MAX_MS (30000U)
#define PRESSES_NEEDED (3U)
#define DEBOUNCE_MS (20U)
#define HEARTBEAT_MS (1000U)
#define POLL_MS (5U)

int ott_button_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0; /* no parameters */
    return 1;
}

int ott_button_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;

    button_init();

    uart_write("Button test: press the USER button (B1 = PC13) three times.\r\n");
    uart_write("Live state below (pressed=1 while held); times out after 30 s.\r\n");

    uint32_t start = millis();
    uint32_t last_beat = start;
    unsigned presses = 0;
    int prev = button_pressed();
    int armed = (prev == 0); /* require a released start so a stuck-low pin can't pass */

    char line[72];
    snprintf(line, sizeof(line), "BTN start pressed=%d\r\n", prev);
    uart_write(line);

    while ((millis() - start) < BUTTON_MAX_MS && presses < PRESSES_NEEDED) {
        int now = button_pressed();
        if (now != prev) {
            delay_ms(DEBOUNCE_MS); /* debounce the edge */
            now = button_pressed();
            if (now != prev) {
                if (!armed && now == 0) {
                    armed = 1;
                }
                if (armed && now == 1) {
                    presses++;
                    snprintf(line, sizeof(line), "BTN press #%u\r\n", presses);
                    uart_write(line);
                }
                prev = now;
            }
        }

        if ((millis() - last_beat) >= HEARTBEAT_MS) {
            last_beat = millis();
            snprintf(line, sizeof(line), "BTN alive pressed=%d presses=%u\r\n",
                     button_pressed(), presses);
            uart_write(line);
        }

        delay_ms(POLL_MS);
    }

    if (presses >= PRESSES_NEEDED) {
        return 1;
    }
    snprintf(reason, reason_size,
             "only %u/%u presses seen (PC13 stuck high? check R-001 button map)",
             presses, PRESSES_NEEDED);
    return 0;
}
