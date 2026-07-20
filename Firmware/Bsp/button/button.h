#ifndef BUTTON_H
#define BUTTON_H

/*
 * On-board user button B1 = PC13 on the NUCLEO-G431RB. On this board B1 is wired
 * ACTIVE-HIGH (pressing drives PC13 to VDD; measured 3.3 V pressed / ~2.4 V idle
 * under CubeMX's pull-up), so button_init() reconfigures PC13 with a pull-DOWN
 * and button_pressed() treats HIGH as pressed. Used to confirm/terminate the
 * manual, button-confirmed OTT scenarios (VT-INT-006/007-style).
 */

void button_init(void);
int  button_pressed(void); /* 1 while the button is held down, else 0 */

/* Block until a fresh press-and-release is seen (with debounce), or until
 * timeout_ms elapses. Returns 1 on press, 0 on timeout. */
int  button_wait_press(unsigned timeout_ms);

#endif /* BUTTON_H */
