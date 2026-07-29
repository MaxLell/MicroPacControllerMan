/*
 * ott.h
 *
 * On-Target Test mechanism. A test is requested on the serial console, survives
 * the reset that follows, and reports its verdict over the same console — so a
 * test runs on real hardware with no debugger attached:
 *
 *   1. `ott <name> [args]` runs the scenario's setup step, stores the request in
 *      retained RAM, and resets the board.
 *   2. On the next boot ott_execute_pending() finds the request, runs the
 *      scenario, prints "OTT PASSED/FAILED [name]", drops the request and
 *      continues into normal operation.
 *
 * This module owns the layout of the retained request; retain_ram only owns the
 * memory it lives in.
 */

#ifndef OTT_H
#define OTT_H

/* ==========================================================================
 * ott - public API
 * ========================================================================= */

/*! \brief Largest parameter blob a scenario may carry across the reset, in bytes. */
#define OTT_PARAMETER_MAX_SIZE (32U)

/*! \brief Register the console commands and bring the command line up.
 *
 * Call once before #ott_execute_pending, which reports its verdict through the
 * command line.
 */
void ott_init(void);

/*! \brief Run a test request left in retained RAM by a previous `ott` command.
 *
 * Call once early in start-up, after #ott_init and before the application starts.
 * Does nothing on a normal, unscheduled boot.
 */
void ott_execute_pending(void);

/*! \brief Feed received console characters to the command line.
 *
 * Call regularly from the main loop.
 */
void ott_poll(void);

#endif /* OTT_H */
