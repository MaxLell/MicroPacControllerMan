#include "app_main.h"

#include "dio_bsp.h"
#include "ott.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "uart_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * app_main - private
 * ========================================================================= */

#define APP_MAIN_BOOT_BANNER \
    "\r\nMicroPacControllerMan booted (M1 U545RE bring-up). Type 'ott' for tests.\r\n"

static void prv_on_systick(void)
{
    (void)user_button_poll();
}

static void prv_init_platform(void)
{
    systick_bsp_init();
    dio_bsp_init();
    uart_bsp_init();
    sw_timer_init();
    user_button_init();

    /* Debouncing needs a steady 1 ms sample rate, so it rides the tick interrupt
     * rather than the main loop. */
    systick_bsp_register_tick_callback(prv_on_systick);
}

/* ==========================================================================
 * app_main - public
 * ========================================================================= */

void app_main(void)
{
    prv_init_platform();

    ott_execute_pending();

    uart_bsp_write_string(APP_MAIN_BOOT_BANNER);

    ott_init();

    for (;;)
    {
        ott_poll();
    }
}
