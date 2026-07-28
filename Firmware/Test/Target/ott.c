#include "ott.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Cli.h"
#include "custom_assert.h"
#include "ott_scenarios.h"
#include "retain_ram.h"
#include "stm32g4xx.h"
#include "uart_bsp.h"

/* ==========================================================================
 * ott - private
 * ========================================================================= */

/* Recognises a deliberately stored request in memory that a power-on reset leaves
 * filled with garbage. */
#define OTT_MAGIC_WORD               (0xB007A5A5U)

/* Test ids are 1-based, so an all-zero retained buffer means "no request". */
#define OTT_TEST_ID_NONE             (0U)
#define OTT_TEST_ID_FIRST            (1U)

/* Plain multiplicative hash — enough to catch a partially written or decayed
 * buffer, not a security measure. */
#define OTT_CHECKSUM_SEED            (0x1234ABCDU)
#define OTT_CHECKSUM_MULTIPLIER      (31U)

/* Long enough for the diagnostic reasons the scenarios actually write; snprintf
 * truncates rather than overruns if one ever outgrows it. */
#define OTT_REASON_MAX_SIZE          (96U)

/* Index of the test name within the argument vector handed to a setup step. */
#define OTT_ARGUMENT_INDEX_NAME      (1)
#define OTT_ARGUMENT_COUNT_WITH_NAME (2)

typedef struct
{
    uint32_t magic_word;
    uint32_t checksum;
    uint32_t test_id;
    uint32_t parameter_size;
    uint8_t parameter[OTT_PARAMETER_MAX_SIZE];
} ott_spec_t;

_Static_assert(sizeof(ott_spec_t) <= RETAIN_RAM_BUFFER_SIZE, "the OTT request must fit into the retained RAM buffer");

static cli_cfg_t g_cli;

/* Covers everything but the magic word and the checksum itself. */
static uint32_t prv_calculate_checksum(const ott_spec_t* const in_spec)
{
    uint32_t checksum = OTT_CHECKSUM_SEED;

    ASSERT(in_spec != NULL);

    checksum = (checksum * OTT_CHECKSUM_MULTIPLIER) + in_spec->test_id;
    checksum = (checksum * OTT_CHECKSUM_MULTIPLIER) + in_spec->parameter_size;

    for (uint32_t index = 0U; (index < in_spec->parameter_size) && (index < OTT_PARAMETER_MAX_SIZE); ++index)
    {
        checksum = (checksum * OTT_CHECKSUM_MULTIPLIER) + in_spec->parameter[index];
    }

    return checksum;
}

static bool prv_is_spec_valid(const ott_spec_t* const in_spec)
{
    ASSERT(in_spec != NULL);

    return (in_spec->magic_word == OTT_MAGIC_WORD) && (in_spec->test_id >= OTT_TEST_ID_FIRST)
           && (in_spec->test_id <= ott_scenarios_get_count()) && (in_spec->parameter_size <= OTT_PARAMETER_MAX_SIZE)
           && (in_spec->checksum == prv_calculate_checksum(in_spec));
}

static void prv_read_spec(ott_spec_t* out_spec)
{
    uint8_t buffer[RETAIN_RAM_BUFFER_SIZE];

    ASSERT(out_spec != NULL);

    retained_ram_read(buffer, sizeof(buffer));
    memcpy(out_spec, buffer, sizeof(*out_spec));
}

static void prv_write_spec(const ott_spec_t* const in_spec)
{
    uint8_t buffer[RETAIN_RAM_BUFFER_SIZE] = {0};

    ASSERT(in_spec != NULL);

    memcpy(buffer, in_spec, sizeof(*in_spec));
    retained_ram_write(buffer, sizeof(buffer));
}

static void prv_invalidate_spec(void)
{
    const uint8_t buffer[RETAIN_RAM_BUFFER_SIZE] = {0};

    retained_ram_write(buffer, sizeof(buffer));
}

static void prv_report(const ott_scenario_t* const in_scenario, bool in_has_passed, const char* const in_reason)
{
    uart_bsp_write_string("OTT ");
    uart_bsp_write_string(in_has_passed ? "PASSED [" : "FAILED [");
    uart_bsp_write_string(in_scenario->name);

    if (in_has_passed)
    {
        uart_bsp_write_string("]\r\n");

        return;
    }

    uart_bsp_write_string("]: ");
    uart_bsp_write_string(in_reason);
    uart_bsp_write_string("\r\n");
}

/* --- `ott <name> [args]`: schedule a test, then reset ---------------------- */

static int prv_ott_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    const ott_scenario_t* scenario;
    ott_spec_t spec = {0};
    size_t index;

    (void)in_context;

    if (in_argument_count < OTT_ARGUMENT_COUNT_WITH_NAME)
    {
        cli_print("OTT tests:");

        for (size_t position = 0U; position < ott_scenarios_get_count(); ++position)
        {
            cli_print("  %s", ott_scenarios_get(position)->name);
        }

        return CLI_OK_STATUS;
    }

    if (!ott_scenarios_find(in_arguments[OTT_ARGUMENT_INDEX_NAME], &index))
    {
        cli_print("OTT ERROR: unknown test '%s'", in_arguments[OTT_ARGUMENT_INDEX_NAME]);

        return CLI_FAIL_STATUS;
    }

    scenario = ott_scenarios_get(index);

    if (scenario->setup_fn != NULL)
    {
        if (!scenario->setup_fn(in_argument_count - OTT_ARGUMENT_INDEX_NAME, &in_arguments[OTT_ARGUMENT_INDEX_NAME],
                                spec.parameter, &spec.parameter_size))
        {
            cli_print("OTT ERROR: setup failed for '%s'", scenario->name);

            return CLI_FAIL_STATUS;
        }
    }

    spec.test_id = (uint32_t)index + OTT_TEST_ID_FIRST;
    spec.magic_word = OTT_MAGIC_WORD;
    spec.checksum = prv_calculate_checksum(&spec);

    prv_write_spec(&spec);

    cli_print("OTT scheduled [%s], resetting...", scenario->name);
    uart_bsp_flush();

    NVIC_SystemReset();

    return CLI_OK_STATUS; /* not reached */
}

/* --- `reset`: reboot into nominal mode, re-emitting the boot banner -------- */

static int prv_reset_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)in_context;

    cli_print("resetting...");
    uart_bsp_flush();

    NVIC_SystemReset();

    return CLI_OK_STATUS; /* not reached */
}

static int prv_cli_put_character(char in_character)
{
    uart_bsp_write_character(in_character);

    return 0;
}

/* ==========================================================================
 * ott - public
 * ========================================================================= */

void ott_execute_pending(void)
{
    const ott_scenario_t* scenario;
    ott_spec_t spec;
    char reason[OTT_REASON_MAX_SIZE];
    bool has_passed;

    prv_read_spec(&spec);

    if (!prv_is_spec_valid(&spec))
    {
        return;
    }

    /* Drop the request BEFORE running it, so a test that crashes the board cannot
     * make it boot-loop into the same test. */
    prv_invalidate_spec();

    scenario = ott_scenarios_get(spec.test_id - OTT_TEST_ID_FIRST);
    reason[0] = '\0';

    has_passed = scenario->run_fn(spec.parameter, spec.parameter_size, reason, sizeof(reason));

    prv_report(scenario, has_passed, reason);
}

void ott_init(void)
{
    cli_binding_t ott_binding = {"ott", prv_ott_command, NULL,
                                 "Schedule an on-target test: ott <name> ('ott' lists them)"};
    cli_binding_t reset_binding = {"reset", prv_reset_command, NULL, "Reboot the board into nominal mode"};

    cli_init(&g_cli, prv_cli_put_character);
    cli_register(&ott_binding);
    cli_register(&reset_binding);
}

void ott_poll(void)
{
    char character;

    while (uart_bsp_read_character(&character))
    {
        cli_receive_and_process(character);
    }
}
