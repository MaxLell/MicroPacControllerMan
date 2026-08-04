/*
 * delay.h
 *
 * Blocking wait. The single place the firmware is allowed to burn time, which is
 * what keeps the cooperative loop honest: there is one function to look at when
 * a frame overruns. Anything that must stay responsive while it waits belongs in
 * sw_timer instead.
 */

#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/* ==========================================================================
 * delay - public API
 * ========================================================================= */

/*! \brief Block for at least the requested number of milliseconds.
 *
 * \param[in]       in_delay_ms: time to wait, in milliseconds
 */
void delay_ms(uint32_t in_delay_ms);

#endif /* DELAY_H */
