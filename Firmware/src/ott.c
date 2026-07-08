#include "ott.h"

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

/* --- registry --- */

typedef struct {
    const char *name;
    ott_run_fn run;
} ott_test_t;

static const ott_test_t k_tests[] = {
    {"blinky", blinky_run},
};
#define OTT_TEST_COUNT (sizeof(k_tests) / sizeof(k_tests[0]))

/* --- dispatch --- */

static void list_tests(void)
{
    uart_write("OTT tests:\r\n");
    for (unsigned i = 0; i < OTT_TEST_COUNT; i++) {
        uart_write("  ");
        uart_write(k_tests[i].name);
        uart_write("\r\n");
    }
}

static void run_named(const char *name)
{
    for (unsigned i = 0; i < OTT_TEST_COUNT; i++) {
        if (strcmp(name, k_tests[i].name) == 0) {
            char reason[64];
            reason[0] = '\0';
            int pass = k_tests[i].run(reason, sizeof(reason));
            if (pass) {
                uart_write("OTT PASSED [");
                uart_write(name);
                uart_write("]\r\n");
            } else {
                uart_write("OTT FAILED [");
                uart_write(name);
                uart_write("]: ");
                uart_write(reason);
                uart_write("\r\n");
            }
            return;
        }
    }
    uart_write("OTT ERROR: unknown test '");
    uart_write(name);
    uart_write("'\r\n");
}

static void dispatch(char *line)
{
    /* trim leading spaces */
    while (*line == ' ') {
        line++;
    }
    if (*line == '\0') {
        return;
    }
    if (strcmp(line, "help") == 0 || strcmp(line, "ott") == 0 ||
        strcmp(line, "ott list") == 0) {
        list_tests();
        return;
    }
    if (strncmp(line, "ott ", 4) == 0) {
        const char *name = line + 4;
        while (*name == ' ') {
            name++;
        }
        run_named(name);
        return;
    }
    uart_write("OTT ERROR: unknown command (try 'ott list')\r\n");
}

void ott_cli_poll(void)
{
    static char buf[64];
    static unsigned len = 0;

    int c;
    while ((c = uart_getc()) >= 0) {
        if (c == '\r' || c == '\n') {
            uart_write("\r\n");
            buf[len] = '\0';
            dispatch(buf);
            len = 0;
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = (char)c;
            char echo[2] = {(char)c, '\0'};
            uart_write(echo); /* echo so interactive typing is visible */
        }
    }
}
