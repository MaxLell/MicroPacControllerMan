#include "ott_touchpad.h"

#include "button.h"
#include "systick.h"
#include "touchpad.h"
#include "uart.h"

#include <stdio.h>
#include <string.h>

/* Safety cap so the board still returns to nominal mode if the operator walks
 * away without pressing the button. */
#define TOUCHPAD_MAX_MS (120000U)
#define SAMPLE_PERIOD_MS (120U)

int ott_touchpad_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0;
    return 1;
}

int ott_touchpad_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;

    button_init();
    touchpad_init();

    if (touchpad_probe() != 0) {
        strncpy(reason, "MTCH6102 not responding on I2C (check slot 2 / SDA-SCL map)", reason_size);
        return 0;
    }

    uart_write("Touchpad live read. Move your finger on the pad; the x/y/touch\r\n");
    uart_write("values below should follow it. Press USER button (B1) to finish.\r\n");
    uart_write("(MTCH6102 reports position + touch-present; it has no Z/pressure.)\r\n");

    uint32_t start = millis();
    uint32_t last = start;
    int armed = 0; /* becomes 1 once the button has first been seen released */

    for (;;) {
        if (!armed && !button_pressed()) {
            armed = 1;
        }
        if (armed && button_pressed()) {
            delay_ms(20);
            if (button_pressed()) {
                break; /* operator confirmed */
            }
        }
        if ((millis() - start) >= TOUCHPAD_MAX_MS) {
            break;
        }

        if ((millis() - last) >= SAMPLE_PERIOD_MS) {
            last = millis();
            uint16_t x = 0, y = 0;
            int touched = 0;
            char line[64];
            if (touchpad_read(&x, &y, &touched) != 0) {
                strncpy(reason, "I2C read failed during live sampling", reason_size);
                return 0;
            }
            snprintf(line, sizeof(line), "TP touch=%d x=%3u y=%3u\r\n", touched,
                     (unsigned)x, (unsigned)y);
            uart_write(line);
        }
    }

    return 1;
}
