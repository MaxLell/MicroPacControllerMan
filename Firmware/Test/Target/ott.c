#include "ott.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Cli.h"
#include "console.h"
#include "crc.h"
#include "custom_assert.h"
#include "ott_scenarios.h"
#include "retain_ram.h"
#include "stm32u5xx.h"

/* ==========================================================================
 * ott - private
 * ========================================================================= */

/* Recognises a deliberately stored request in memory that a power-on reset leaves
 * filled with garbage. */
#define OTT_MAGIC_WORD               (0xB007A5A5U)

/* Test ids are 1-based, so an all-zero retained buffer means "no request". */
#define OTT_TEST_ID_FIRST            (1U)

/* Long enough for the diagnostic reasons the scenarios actually write; snprintf
 * truncates rather than overruns if one ever outgrows it. */
#define OTT_REASON_MAX_SIZE          (96U)

/* Index of the test name within the argument vector handed to a setup step. */
#define OTT_ARGUMENT_INDEX_NAME      (1)
#define OTT_ARGUMENT_COUNT_WITH_NAME (2)

/* The part of the request the checksum covers, kept in its own struct so it is one
 * contiguous range and a single CRC call spans it. */
typedef struct
{
    uint32_t test_id;
    uint8_t parameter[OTT_PARAMETER_MAX_SIZE];
} ott_request_t;

typedef struct
{
    uint32_t magic_word;
    uint32_t checksum;
    ott_request_t request;
} ott_spec_t;

_Static_assert(sizeof(ott_spec_t) <= RETAIN_RAM_BUFFER_SIZE, "the OTT request must fit into the retained RAM buffer");

static cli_cfg_t g_cli;

static uint32_t prv_calculate_checksum(const ott_spec_t* const in_spec)
{
    ASSERT(in_spec != NULL);

    return crc_32((const uint8_t*)&in_spec->request, sizeof(in_spec->request));
}

static bool prv_is_spec_valid(const ott_spec_t* const in_spec)
{
    ASSERT(in_spec != NULL);

    return (in_spec->magic_word == OTT_MAGIC_WORD) && (in_spec->request.test_id >= OTT_TEST_ID_FIRST)
           && (in_spec->request.test_id <= ott_scenarios_get_count())
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
    ASSERT(in_scenario != NULL);
    ASSERT(in_reason != NULL);

    if (in_has_passed)
    {
        cli_print("OTT PASSED [%s]", in_scenario->name);
    }
    else
    {
        cli_print("OTT FAILED [%s]: %s", in_scenario->name, in_reason);
    }
}

/* --- `ott <name> [args]`: schedule a test, then reset ---------------------- */

static void prv_print_scenario_list(void)
{
    cli_print("OTT tests:");

    for (size_t position = 0U; position < ott_scenarios_get_count(); ++position)
    {
        cli_print("  %s", ott_scenarios_get(position)->name);
    }
}

static bool prv_build_request(const ott_scenario_t* const in_scenario, size_t in_index, int in_argument_count,
                              char* in_arguments[], ott_request_t* out_request)
{
    bool is_built = true;

    ASSERT(in_scenario != NULL);
    ASSERT(in_arguments != NULL);
    ASSERT(out_request != NULL);

    out_request->test_id = (uint32_t)in_index + OTT_TEST_ID_FIRST;

    if (in_scenario->setup_fn != NULL)
    {
        is_built = in_scenario->setup_fn(in_argument_count - OTT_ARGUMENT_INDEX_NAME,
                                         &in_arguments[OTT_ARGUMENT_INDEX_NAME], out_request->parameter);
    }

    return is_built;
}

/* Stores the request where it survives the reset, then triggers it. Does not return. */
static void prv_schedule_and_reset(const ott_scenario_t* const in_scenario, const ott_request_t* const in_request)
{
    ott_spec_t spec = {0};

    ASSERT(in_scenario != NULL);
    ASSERT(in_request != NULL);

    spec.magic_word = OTT_MAGIC_WORD;
    spec.request = *in_request;
    spec.checksum = prv_calculate_checksum(&spec);

    prv_write_spec(&spec);

    cli_print("OTT scheduled [%s], resetting...", in_scenario->name);
    console_flush();

    NVIC_SystemReset();
}

static int prv_ott_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    int status = CLI_OK_STATUS;
    ott_request_t request = {0};
    size_t index;

    (void)in_context;

    if (in_argument_count < OTT_ARGUMENT_COUNT_WITH_NAME)
    {
        prv_print_scenario_list();
    }
    else if (!ott_scenarios_find(in_arguments[OTT_ARGUMENT_INDEX_NAME], &index))
    {
        cli_print("OTT ERROR: unknown test '%s'", in_arguments[OTT_ARGUMENT_INDEX_NAME]);
        status = CLI_FAIL_STATUS;
    }
    else
    {
        const ott_scenario_t* const scenario = ott_scenarios_get(index);

        if (prv_build_request(scenario, index, in_argument_count, in_arguments, &request))
        {
            prv_schedule_and_reset(scenario, &request);
        }
        else
        {
            cli_print("OTT ERROR: setup failed for '%s'", scenario->name);
            status = CLI_FAIL_STATUS;
        }
    }

    return status;
}

/* --- `reset`: reboot into nominal mode, re-emitting the boot banner -------- */

static int prv_reset_command(int in_argument_count, char* in_arguments[], void* in_context)
{
    (void)in_argument_count;
    (void)in_arguments;
    (void)in_context;

    cli_print("resetting...");
    console_flush();

    NVIC_SystemReset();

    return CLI_OK_STATUS; /* not reached */
}

static int prv_cli_put_character(char in_character)
{
    console_write_character(in_character);

    return 0;
}

/* ==========================================================================
 * ott - public
 * ========================================================================= */

void ott_init(void)
{
    cli_binding_t ott_binding = {"ott", prv_ott_command, NULL,
                                 "Schedule an on-target test: ott <name> ('ott' lists them)"};
    cli_binding_t reset_binding = {"reset", prv_reset_command, NULL, "Reboot the board into nominal mode"};

    cli_init(&g_cli, prv_cli_put_character);
    cli_register(&ott_binding);
    cli_register(&reset_binding);
}

void ott_execute_pending(void)
{
    ott_spec_t spec;

    prv_read_spec(&spec);

    if (prv_is_spec_valid(&spec))
    {
        const ott_scenario_t* const scenario = ott_scenarios_get(spec.request.test_id - OTT_TEST_ID_FIRST);
        char reason[OTT_REASON_MAX_SIZE] = {'\0'};
        bool has_passed;

        /* Drop the request BEFORE running it, so a test that crashes the board cannot
         * make it boot-loop into the same test. */
        prv_invalidate_spec();

        has_passed = scenario->run_fn(spec.request.parameter, reason, sizeof(reason));

        prv_report(scenario, has_passed, reason);
    }
}

void ott_poll(void)
{
    char character;

    while (console_read_character(&character))
    {
        cli_receive_and_process(character);
    }
}
