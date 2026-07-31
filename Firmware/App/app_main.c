#include "app_main.h"

#include "Cli.h"
#include "console.h"
#include "dio_bsp.h"
#include "joystick.h"
#include "ott.h"
#include "spi_bsp.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * app_main - private
 * ========================================================================= */

#define APP_MAIN_BOOT_BANNER "MicroPacControllerMan booted (M1 U545RE bring-up). Type 'ott' for tests."

static void prv_on_systick(void)
{
    (void)user_button_poll();
    joystick_poll();
}

static void prv_init_platform(void)
{
    systick_bsp_init();
    dio_bsp_init();
    console_init();
    spi_bsp_init();
    sw_timer_init();
    user_button_init();
    joystick_init();

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

    /* The command line comes up first: it clears the screen, and everything below
     * reports through it — including a pending test's verdict. */
    ott_init();

    cli_print(APP_MAIN_BOOT_BANNER);

    ott_execute_pending();

    for (;;)
    {
        ott_poll();
    }
}
