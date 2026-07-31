/*
 * sw_timer.h
 *
 * Non-blocking software timers on top of the millisecond tick. A caller arms a
 * timer and keeps doing other work; sw_timer_process() fires the callbacks of
 * whichever timers have expired. Replaces open-coded tick arithmetic.
 *
 * Timers are one-shot: a timer goes inactive when it fires. A callback that
 * re-arms its own timer makes it periodic.
 */

#ifndef SW_TIMER_H
#define SW_TIMER_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * sw_timer - public types
 * ========================================================================= */

#define SW_TIMER_MAX_TIMERS (8U)

typedef void (*sw_timer_callback_fn)(void);

typedef struct
{
    uint32_t start_tick;              /*!< Tick the timer was armed at              */
    uint32_t timeout_ms;              /*!< Time until expiry, in milliseconds       */
    sw_timer_callback_fn callback_fn; /*!< Called once on expiry                    */
    bool is_active;                   /*!< Armed and not yet expired                */
} sw_timer_t;

/* ==========================================================================
 * sw_timer - public API
 * ========================================================================= */

/*! \brief Initialize the software timer service. Must be called exactly once. */
void sw_timer_init(void);

/*! \brief Register a timer instance with the service.
 *
 * Resets the instance to inactive. At most #SW_TIMER_MAX_TIMERS instances can be
 * registered at a time.
 *
 * \param[in,out]   in_timer: instance to register, must not be `NULL`
 */
void sw_timer_create(sw_timer_t* in_timer);

/*! \brief Arm a registered timer.
 *
 * \param[in,out]   in_timer: registered instance to arm
 * \param[in]       in_timeout_ms: time until expiry, in milliseconds
 * \param[in]       in_callback_fn: called from #sw_timer_process on expiry
 */
void sw_timer_start(sw_timer_t* in_timer, uint32_t in_timeout_ms, sw_timer_callback_fn in_callback_fn);

/*! \brief Restart a running timer from the current tick, keeping its timeout.
 *
 * \param[in,out]   in_timer: registered, armed instance
 */
void sw_timer_reset(sw_timer_t* in_timer);

/*! \brief Disarm a timer without firing its callback.
 *
 * \param[in,out]   in_timer: registered instance
 */
void sw_timer_stop(sw_timer_t* in_timer);

/*! \brief Report whether a timer is armed and has not yet expired.
 *
 * Lets a caller use a timer as a plain deadline: arm it, then loop while it is
 * still active.
 *
 * \param[in]       in_timer: registered instance
 * \return          `true` while the timer is armed and unexpired
 */
bool sw_timer_is_active(const sw_timer_t* in_timer);

/*! \brief Fire the callbacks of all expired timers.
 *
 * Call regularly from the main loop. Runs in caller context, so callbacks may do
 * real work (logging, I/O).
 */
void sw_timer_process(void);

#endif /* SW_TIMER_H */
