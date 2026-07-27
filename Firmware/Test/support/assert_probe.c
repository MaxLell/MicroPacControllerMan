#include "assert_probe.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "unity.h"

/* ==========================================================================
 * assert_probe - private
 * ========================================================================= */

static const char* g_expression = NULL;
static uint32_t g_count = 0U;
static bool g_is_armed = false;
static bool g_has_aborted = false;

static void prv_on_assert_failed(const char* file, uint32_t line, const char* expr)
{
    (void)file;
    (void)line;

    g_expression = expr;
    ++g_count;

    if (g_is_armed)
    {
        g_is_armed = false;
        g_has_aborted = true;

        /* Unwind to the enclosing TEST_PROTECT(), so the caller does not continue
         * past a violated precondition. */
        TEST_ABORT();
    }
}

/* ==========================================================================
 * assert_probe - public
 * ========================================================================= */

void assert_probe_begin(void)
{
    g_expression = NULL;
    g_count = 0U;
    g_is_armed = false;
    g_has_aborted = false;

    custom_assert_init(prv_on_assert_failed);
}

void assert_probe_end(void)
{
    custom_assert_deinit();
}

uint32_t assert_probe_get_count(void)
{
    return g_count;
}

const char* assert_probe_get_expression(void)
{
    return g_expression;
}

void assert_probe_arm(void)
{
    g_expression = NULL;
    g_count = 0U;
    g_is_armed = true;
    g_has_aborted = false;
}

bool assert_probe_has_aborted(void)
{
    return g_has_aborted;
}
