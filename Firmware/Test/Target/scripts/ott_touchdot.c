#include "ott_touchdot.h"

#include "button.h"
#include "display.h"
#include "gfx.h"
#include "systick.h"
#include "touchpad.h"
#include "uart.h"

#include <string.h>

#define W DISPLAY_WIDTH
#define H DISPLAY_HEIGHT
#define DOT_R 5
#define TOUCHDOT_MAX_MS (120000U)
#define FRAME_PERIOD_MS (40U)

int ott_touchdot_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0;
    return 1;
}

int ott_touchdot_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;

    button_init();
    display_init();
    touchpad_init();

    if (touchpad_probe() != 0) {
        strncpy(reason, "MTCH6102 not responding on I2C (check slot 2 / SDA-SCL map)", reason_size);
        return 0;
    }

    uart_write("Touch-dot test: the dot on the LCD follows your finger on the pad.\r\n");
    uart_write("Press USER button (B1) to finish.\r\n");

    uint32_t start = millis();
    uint32_t last_vcom = start;
    int armed = 0;

    for (;;) {
        if (!armed && !button_pressed()) {
            armed = 1;
        }
        if (armed && button_pressed()) {
            delay_ms(20);
            if (button_pressed()) {
                break;
            }
        }
        if ((millis() - start) >= TOUCHDOT_MAX_MS) {
            break;
        }

        uint16_t x = 0, y = 0;
        int touched = 0;
        if (touchpad_read(&x, &y, &touched) != 0) {
            strncpy(reason, "I2C read failed during tracking", reason_size);
            return 0;
        }

        gfx_fill(DISPLAY_WHITE);
        gfx_rect(0, 0, W, H, DISPLAY_BLACK);
        if (touched) {
            /* Map raw touch range to the 128x128 panel. Orientation may need a
             * flip once confirmed on hardware (noted in the M2 docs). */
            int sx = (int)((uint32_t)x * (W - 1) / TOUCHPAD_X_MAX);
            int sy = (int)((uint32_t)y * (H - 1) / TOUCHPAD_Y_MAX);
            gfx_fill_circle(sx, sy, DOT_R, DISPLAY_BLACK);
            gfx_hline(0, sy, W, DISPLAY_BLACK);
            gfx_vline(sx, 0, H, DISPLAY_BLACK);
        }
        display_flush();

        if ((millis() - last_vcom) >= 1000U) {
            last_vcom = millis();
            display_vcom_tick();
        }
        delay_ms(FRAME_PERIOD_MS);
    }

    return 1;
}
