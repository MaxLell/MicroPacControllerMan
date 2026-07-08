#ifndef OTT_H
#define OTT_H

/*
 * On-Target Test (OTT) layer (FR-106 / FR-107), built on the EmbeddedCli
 * framework (third_party/embedded_cli).
 *
 * Exposes `ott <name>` on the serial console; the firmware runs the named test
 * and prints a machine-parseable result without a debugger:
 *     OTT PASSED [<name>]
 *     OTT FAILED [<name>]: <reason>
 * `ott` with no argument lists the available tests; `help` lists CLI commands.
 *
 * Tests that need a clean reset state (retained-RAM/reset, doc 09) are added in
 * a later milestone; the tests here run in place.
 */

void ott_init(void); /* set up the CLI and register the OTT command */
void ott_poll(void); /* drain UART RX into the CLI; call from the main loop */

#endif /* OTT_H */
