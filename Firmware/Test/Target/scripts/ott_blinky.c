#include "ott_blinky.h"

#include "led.h"

#include <string.h>

int ott_blinky_setup(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size)
{
    (void)argc;
    (void)argv;
    (void)out_data;
    *out_data_size = 0; /* no parameters */
    return 1;
}

static void short_settle(void)
{
    for (volatile int k = 0; k < 100; k++) {
    }
}

int ott_blinky_run(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size)
{
    (void)data;
    (void)data_size;

    led_init();
    for (int i = 0; i < 5; i++) {
        led_set(1);
        short_settle();
        if (led_get() != 1) {
            strncpy(reason, "PA5 read low while driven high", reason_size);
            return 0;
        }
        led_set(0);
        short_settle();
        if (led_get() != 0) {
            strncpy(reason, "PA5 read high while driven low", reason_size);
            return 0;
        }
    }
    return 1;
}
