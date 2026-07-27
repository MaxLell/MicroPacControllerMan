/*
 * app_main.h
 *
 * Firmware entry point.
 */

#ifndef APP_MAIN_H
#define APP_MAIN_H

/* ==========================================================================
 * app_main - public API
 * ========================================================================= */

/*! \brief Run the firmware. Never returns.
 *
 * Called once from the STM32CubeMX-generated main(), after HAL_Init(), the clock
 * setup and the MX_*_Init() peripheral bring-up. Initializes the platform
 * modules, runs any pending on-target test, prints the boot banner, then enters
 * the nominal super-loop.
 */
void app_main(void);

#endif /* APP_MAIN_H */
