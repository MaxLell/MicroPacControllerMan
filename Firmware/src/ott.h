#ifndef OTT_H
#define OTT_H

/*
 * On-Target Test (OTT) CLI framework (FR-106 / FR-107).
 *
 * A test is exposed as `ott <name>` on the serial console. The firmware runs it
 * and prints a machine-parseable result WITHOUT a debugger attached:
 *     OTT PASSED [<name>]
 *     OTT FAILED [<name>]: <reason>
 * `ott list` prints the available tests.
 *
 * Call ott_cli_poll() repeatedly from the main loop; it drains the UART RX and
 * dispatches each complete command line. Tests that need a clean reset state
 * (retained-RAM/reset, doc 09) are added in a later milestone; the tests here
 * run in place.
 */
void ott_cli_poll(void);

#endif /* OTT_H */
