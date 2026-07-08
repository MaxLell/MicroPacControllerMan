#ifndef LED_H
#define LED_H

/* On-board LED LD2 (PA5) on the STM32G431RB Nucleo-64. */
void led_init(void);
void led_set(int on);   /* drive the pin high (1) / low (0) */
int  led_get(void);     /* actual pin level read back via the input register (0/1) */
void led_toggle(void);

#endif /* LED_H */
