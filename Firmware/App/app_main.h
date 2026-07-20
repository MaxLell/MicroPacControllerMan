#ifndef APP_MAIN_H
#define APP_MAIN_H

/*
 * Firmware entry point, called once from the STM32CubeMX-generated main() (from
 * its USER CODE BEGIN 2 block, after HAL_Init / clock / MX_*_Init). Runs any
 * pending On-Target Test, prints the boot banner (VT-INT-002), then enters the
 * nominal super-loop (~1 Hz LD2 blink + OTT CLI polling on the LPUART1 VCP).
 * Never returns.
 */
void app_main(void);

#endif /* APP_MAIN_H */
