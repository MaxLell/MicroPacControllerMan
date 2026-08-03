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
 *      scenario, prints "OTT PASSED/FAILED [name]" and drops the request.
 *
 * A scheduled test therefore *replaces* that boot's normal operation rather than
 * preceding it: the reset interrupts whatever was running, and `reset` is what
 * puts the board back into the game.
 *
 * This module owns the layout of the retained request; retain_ram only owns the
 * memory it lives in.
 */

#ifndef OTT_H
#define OTT_H

#include <stdbool.h>

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
 *
 * The return value is what tells the two boots apart, and the caller has to act on it:
 * scheduling a test resets the board, so a test run and the game starting are alternatives
 * rather than a sequence. After a test the board belongs to whoever asked for it — the
 * harness is waiting on the verdict over the same serial line, and `reset` is what brings
 * the game back ([09 OTT Mechanism](../../../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)).
 *
 * \return          `true` when a test was pending and has now run
 */
bool ott_execute_pending(void);

/*! \brief Feed received console characters to the command line.
 *
 * Call regularly from the main loop.
 */
void ott_poll(void);

#endif /* OTT_H */
