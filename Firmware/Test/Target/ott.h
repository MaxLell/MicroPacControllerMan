#ifndef OTT_H
#define OTT_H

/*
 * On-Target Test (OTT) mechanism (FR-106 / FR-107), following the reference
 * design (BareMetalHollowClockFw, doc 09), on top of the EmbeddedCli console:
 *
 *   1. `ott <name> [args]` on the CLI runs the scenario's setup step, writes the
 *      request (with a magic word + checksum) into retained RAM, and resets.
 *   2. On the next boot, ott_execute_pending() finds a valid request, runs the
 *      scenario's run step, and prints "OTT PASSED/FAILED [name]" over the serial
 *      console (no debugger needed), then clears the request and continues into
 *      normal operation.
 *
 * Tests live in their own ott_<name>.c modules and are listed in ott_scenarios.c
 * — adding a test needs no change to this core or the CLI.
 */

/* Run a pending OTT request left in retained RAM by a prior `ott` command.
 * Call once early in main(), after the LED/UART are initialised and before the
 * normal application starts. No-op on a normal (unscheduled) boot. */
void ott_execute_pending(void);

/* Register the `ott` CLI command (and set up the console). */
void ott_init(void);

/* Drain the UART RX into the CLI; call from the main loop. */
void ott_poll(void);

#endif /* OTT_H */
