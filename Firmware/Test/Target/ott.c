#include "ott.h"

#include "Cli.h"
#include "ott_scenarios.h"
#include "retain_ram.h"
#include "uart.h"

#include "stm32g4xx.h" /* NVIC_SystemReset */

#include <string.h>

#define OTT_MAGIC (0xB007A5A5U)

/* Checksum over the meaningful spec fields (everything except magic/checksum). */
static uint32_t prv_checksum(const ott_spec_t* s)
{
    uint32_t c = 0x1234ABCDU;
    c = c * 31U + s->test_id;
    c = c * 31U + s->data_size;
    for (uint32_t i = 0; i < s->data_size && i < OTT_ARG_MAX; i++) {
        c = c * 31U + s->data[i];
    }
    return c;
}

static int prv_spec_is_valid(const ott_spec_t* s)
{
    return (s->magic == OTT_MAGIC) && (s->test_id >= 1U) &&
           (s->test_id <= ott_scenarios_count()) && (s->data_size <= OTT_ARG_MAX) &&
           (s->checksum == prv_checksum(s));
}

void ott_execute_pending(void)
{
    ott_spec_t* s = retain_ott_spec();

    if (!prv_spec_is_valid(s)) {
        return; /* normal boot: no (valid) request pending */
    }

    const ott_scenario_t* sc = ott_scenarios_get((unsigned)(s->test_id - 1U));

    /* Invalidate the request BEFORE running, so a crash mid-test cannot loop-boot
     * into the same test. */
    uint32_t data_size = s->data_size;
    uint8_t data[OTT_ARG_MAX];
    memcpy(data, s->data, sizeof(data));
    s->magic = 0U;

    char reason[64];
    reason[0] = '\0';
    int pass = sc->run(data, data_size, reason, sizeof(reason));

    /* Report over the serial console, before returning to normal operation. */
    uart_write("OTT ");
    uart_write(pass ? "PASSED [" : "FAILED [");
    uart_write(sc->name);
    if (pass) {
        uart_write("]\r\n");
    } else {
        uart_write("]: ");
        uart_write(reason);
        uart_write("\r\n");
    }
}

/* --- `ott <name> [args]` CLI command: schedule a test, then reset --- */

static int ott_cmd(int argc, char* argv[], void* context)
{
    (void)context;

    if (argc < 2) {
        cli_print("OTT tests:");
        for (unsigned i = 0; i < ott_scenarios_count(); i++) {
            cli_print("  %s", ott_scenarios_get(i)->name);
        }
        return CLI_OK_STATUS;
    }

    int idx = ott_scenarios_find(argv[1]);
    if (idx < 0) {
        cli_print("OTT ERROR: unknown test '%s'", argv[1]);
        return CLI_FAIL_STATUS;
    }

    const ott_scenario_t* sc = ott_scenarios_get((unsigned)idx);
    ott_spec_t* s = retain_ott_spec();
    s->data_size = 0;
    memset(s->data, 0, sizeof(s->data));

    /* setup runs the CLI-side argument parsing/validation */
    if (sc->setup != 0) {
        if (!sc->setup(argc - 1, &argv[1], s->data, &s->data_size)) {
            cli_print("OTT ERROR: setup failed for '%s'", sc->name);
            return CLI_FAIL_STATUS;
        }
    }

    s->test_id = (uint32_t)idx + 1U;
    s->magic = OTT_MAGIC;
    s->checksum = prv_checksum(s);

    cli_print("OTT scheduled [%s], resetting...", sc->name);
    uart_flush(); /* make sure the message is fully sent before we reset */
    NVIC_SystemReset();

    return CLI_OK_STATUS; /* not reached */
}

/* --- `reset` CLI command: reboot into nominal mode (re-emits the boot banner,
 * used by the harness for VT-INT-002) --- */

static int reset_cmd(int argc, char* argv[], void* context)
{
    (void)argc;
    (void)argv;
    (void)context;
    cli_print("resetting...");
    uart_flush();
    NVIC_SystemReset();
    return CLI_OK_STATUS; /* not reached */
}

/* --- console glue (EmbeddedCli) --- */

static cli_cfg_t g_cli;

static int cli_put(char c) { return uart_putc(c); }

void ott_init(void)
{
    cli_init(&g_cli, cli_put);
    cli_binding_t ott_binding = {"ott", ott_cmd, NULL, "Schedule an on-target test: ott <name> ('ott' lists them)"};
    cli_register(&ott_binding);
    cli_binding_t reset_binding = {"reset", reset_cmd, NULL, "Reboot the board into nominal mode"};
    cli_register(&reset_binding);
}

void ott_poll(void)
{
    int c;
    while ((c = uart_getc()) >= 0) {
        cli_receive_and_process((char)c);
    }
}
