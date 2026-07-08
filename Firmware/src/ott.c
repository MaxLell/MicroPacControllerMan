#include "ott.h"

#include "Cli.h"
#include "led.h"
#include "uart.h"

#include <string.h>

/* --- test runners: return 1 = PASS, 0 = FAIL (with a reason written out) --- */

typedef int (*ott_run_fn)(char *reason, unsigned reason_size);

static void short_settle(void)
{
    for (volatile int k = 0; k < 100; k++) {
    }
}

/*
 * blinky: prove the LED (PA5) can be driven and that the pin actually follows,
 * by reading the level back from the input register over several cycles. This is
 * the firmware-observable check that the blink mechanism works (VT-INT-005).
 */
static int blinky_run(char *reason, unsigned reason_size)
{
    for (int i = 0; i < 5; i++) {
        led_set(1);
        short_settle();
        if (led_get() != 1) {
            strncpy(reason, "PA5 read low while driven high", reason_size);
            return 0;
        }
        led_set(0);
        short_settle();
        if (led_get() != 0) {
            strncpy(reason, "PA5 read high while driven low", reason_size);
            return 0;
        }
    }
    return 1;
}

/* --- test registry --- */

typedef struct {
    const char *name;
    ott_run_fn run;
} ott_test_t;

static const ott_test_t k_tests[] = {
    {"blinky", blinky_run},
};
#define OTT_TEST_COUNT (sizeof(k_tests) / sizeof(k_tests[0]))

/* --- `ott <name>` CLI command --- */

static int ott_cmd(int argc, char *argv[], void *context)
{
    (void)context;

    if (argc < 2) {
        cli_print("OTT tests:");
        for (unsigned i = 0; i < OTT_TEST_COUNT; i++) {
            cli_print("  %s", k_tests[i].name);
        }
        return CLI_OK_STATUS;
    }

    for (unsigned i = 0; i < OTT_TEST_COUNT; i++) {
        if (strcmp(argv[1], k_tests[i].name) == 0) {
            char reason[64];
            reason[0] = '\0';
            int pass = k_tests[i].run(reason, sizeof(reason));
            if (pass) {
                cli_print("OTT PASSED [%s]", k_tests[i].name);
                return CLI_OK_STATUS;
            }
            cli_print("OTT FAILED [%s]: %s", k_tests[i].name, reason);
            return CLI_FAIL_STATUS;
        }
    }

    cli_print("OTT ERROR: unknown test '%s'", argv[1]);
    return CLI_FAIL_STATUS;
}

/* --- glue --- */

static cli_cfg_t g_cli;

static int cli_put(char c) { return uart_putc(c); }

void ott_init(void)
{
    cli_init(&g_cli, cli_put);
    cli_binding_t ott_binding = {"ott", ott_cmd, NULL, "Run an on-target test: ott <name> ('ott' lists them)"};
    cli_register(&ott_binding);
}

void ott_poll(void)
{
    int c;
    while ((c = uart_getc()) >= 0) {
        cli_receive_and_process((char)c);
    }
}
