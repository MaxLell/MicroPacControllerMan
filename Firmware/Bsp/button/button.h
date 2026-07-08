#ifndef BUTTON_H
#define BUTTON_H

/*
 * On-board user button B1 = PC13 on the NUCLEO-G431RB (active-low; an internal
 * pull-up is enabled so the idle level is high). Used to confirm/terminate the
 * manual, button-confirmed OTT scenarios (VT-INT-006/007-style).
 */

void button_init(void);
int  button_pressed(void); /* 1 while the button is held down, else 0 */

/* Block until a fresh press-and-release is seen (with debounce), or until
 * timeout_ms elapses. Returns 1 on press, 0 on timeout. */
int  button_wait_press(unsigned timeout_ms);

#endif /* BUTTON_H */
