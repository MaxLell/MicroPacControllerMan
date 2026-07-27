/*
 * Unit tests for Services/sw_timer.
 *
 * The tick is mocked, so expiry is driven exactly rather than waited for. Each test
 * starts from a clean module via the TEST-only sw_timer_test_reset() hook, because
 * sw_timer_init() deliberately asserts when called twice.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert_probe.h"
/* Ceedling picks the sources to link from the includes it sees in this file, and
 * both sw_timer and the assert probe need custom_assert. Without this the link
 * fails — see the note in project.yml. */
#include "custom_assert.h"
#include "mock_systick_bsp.h"
#include "sw_timer.h"
#include "unity.h"

/* Compiled in only under TEST — see sw_timer.c. */
void sw_timer_test_reset(void);

#define TEST_TIMEOUT_MS (100U)
#define TEST_START_TICK (1000U)

#define TEST_ONE_CALL (1U)
#define TEST_NO_CALLS (0U)
#define TEST_REARM_COUNT (2U)
#define TEST_EXPECTED_PERIODIC_CALLS (TEST_REARM_COUNT + 1U)

/* Multiples of the timeout used to prove a one-shot timer stays silent afterwards. */
#define TEST_WELL_PAST_TIMEOUT (TEST_TIMEOUT_MS * 5U)
#define TEST_MUCH_LATER (TEST_TIMEOUT_MS * 9U)
#define TEST_THREE_TIMEOUTS (TEST_TIMEOUT_MS * 3U)

static uint32_t g_callback_calls;
static uint32_t g_rearm_budget;
static sw_timer_t g_timer;

static void prv_on_expiry(void)
{
    ++g_callback_calls;
}

static void prv_on_expiry_rearming(void)
{
    ++g_callback_calls;

    if (g_rearm_budget > 0U)
    {
        --g_rearm_budget;

        systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK);
        sw_timer_start(&g_timer, TEST_TIMEOUT_MS, prv_on_expiry_rearming);
    }
}

/* Arm g_timer at a known tick. */
static void prv_start_timer(sw_timer_callback_fn in_callback_fn)
{
    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK);
    sw_timer_start(&g_timer, TEST_TIMEOUT_MS, in_callback_fn);
}

/* Run one sw_timer_process() with the tick reporting `in_elapsed_ms` since the arm. */
static void prv_process_at_elapsed(uint32_t in_elapsed_ms)
{
    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK + in_elapsed_ms);
    sw_timer_process();
}

void setUp(void)
{
    assert_probe_begin();

    sw_timer_test_reset();
    sw_timer_init();

    g_callback_calls = 0U;
    g_rearm_budget = 0U;

    sw_timer_create(&g_timer);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- arming and expiry ---------------------------------------------------- */

void test_a_created_timer_is_inactive(void)
{
    TEST_ASSERT_FALSE(sw_timer_is_active(&g_timer));
}

void test_an_armed_timer_is_active(void)
{
    prv_start_timer(prv_on_expiry);

    TEST_ASSERT_TRUE(sw_timer_is_active(&g_timer));
}

void test_a_timer_does_not_fire_before_its_timeout(void)
{
    prv_start_timer(prv_on_expiry);

    prv_process_at_elapsed(TEST_TIMEOUT_MS - 1U);

    TEST_ASSERT_EQUAL_UINT32(TEST_NO_CALLS, g_callback_calls);
    TEST_ASSERT_TRUE(sw_timer_is_active(&g_timer));
}

void test_a_timer_fires_exactly_on_its_timeout(void)
{
    prv_start_timer(prv_on_expiry);

    prv_process_at_elapsed(TEST_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_CALL, g_callback_calls);
}

void test_a_timer_goes_inactive_once_it_has_fired(void)
{
    prv_start_timer(prv_on_expiry);

    prv_process_at_elapsed(TEST_TIMEOUT_MS);

    TEST_ASSERT_FALSE(sw_timer_is_active(&g_timer));
}

void test_a_one_shot_timer_fires_only_once(void)
{
    prv_start_timer(prv_on_expiry);

    prv_process_at_elapsed(TEST_TIMEOUT_MS);
    prv_process_at_elapsed(TEST_WELL_PAST_TIMEOUT);
    prv_process_at_elapsed(TEST_MUCH_LATER);

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_CALL, g_callback_calls);
}

void test_expiry_survives_tick_wraparound(void)
{
    systick_bsp_get_tick_ExpectAndReturn(UINT32_MAX - 1U);
    sw_timer_start(&g_timer, TEST_TIMEOUT_MS, prv_on_expiry);

    /* One millisecond after the arm, but on the far side of the wrap. */
    systick_bsp_get_tick_ExpectAndReturn(0U);
    sw_timer_process();

    TEST_ASSERT_EQUAL_UINT32(TEST_NO_CALLS, g_callback_calls);

    systick_bsp_get_tick_ExpectAndReturn(TEST_TIMEOUT_MS - 2U);
    sw_timer_process();

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_CALL, g_callback_calls);
}

/* --- re-arming and stopping ----------------------------------------------- */

void test_a_callback_that_rearms_its_timer_makes_it_periodic(void)
{
    g_rearm_budget = TEST_REARM_COUNT;

    prv_start_timer(prv_on_expiry_rearming);

    prv_process_at_elapsed(TEST_TIMEOUT_MS);
    prv_process_at_elapsed(TEST_TIMEOUT_MS);
    prv_process_at_elapsed(TEST_TIMEOUT_MS);

    TEST_ASSERT_EQUAL_UINT32(TEST_EXPECTED_PERIODIC_CALLS, g_callback_calls);
    TEST_ASSERT_FALSE(sw_timer_is_active(&g_timer));
}

void test_a_stopped_timer_never_fires(void)
{
    prv_start_timer(prv_on_expiry);
    sw_timer_stop(&g_timer);

    prv_process_at_elapsed(TEST_THREE_TIMEOUTS);

    TEST_ASSERT_EQUAL_UINT32(TEST_NO_CALLS, g_callback_calls);
    TEST_ASSERT_FALSE(sw_timer_is_active(&g_timer));
}

void test_reset_restarts_the_timeout_from_the_current_tick(void)
{
    prv_start_timer(prv_on_expiry);

    /* Re-base to a tick one timeout later, so the original deadline is in the past
     * but the new one is not. */
    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK + TEST_TIMEOUT_MS);
    sw_timer_reset(&g_timer);

    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK + TEST_TIMEOUT_MS
                                        + (TEST_TIMEOUT_MS - 1U));
    sw_timer_process();

    TEST_ASSERT_EQUAL_UINT32(TEST_NO_CALLS, g_callback_calls);

    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK + (TEST_TIMEOUT_MS * 2U));
    sw_timer_process();

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_CALL, g_callback_calls);
}

/* --- preconditions -------------------------------------------------------- */

void test_creating_a_null_timer_asserts(void)
{
    ASSERT_PROBE_EXPECT(sw_timer_create(NULL), "in_timer != NULL");
}

void test_creating_the_same_timer_twice_asserts(void)
{
    ASSERT_PROBE_EXPECT(sw_timer_create(&g_timer), "false == prv_is_registered_timer(in_timer)");
}

void test_starting_an_unregistered_timer_asserts(void)
{
    static sw_timer_t unregistered;

    ASSERT_PROBE_EXPECT(sw_timer_start(&unregistered, TEST_TIMEOUT_MS, prv_on_expiry),
                        "prv_is_registered_timer(in_timer)");
}

void test_starting_a_timer_without_a_callback_asserts(void)
{
    ASSERT_PROBE_EXPECT(sw_timer_start(&g_timer, TEST_TIMEOUT_MS, NULL),
                        "in_callback_fn != NULL");
}

void test_exhausting_the_timer_table_asserts(void)
{
    static sw_timer_t timers[SW_TIMER_MAX_TIMERS];

    /* setUp already registered one, so the table fills one short of the array. */
    for (size_t index = 0U; index < (SW_TIMER_MAX_TIMERS - 1U); ++index)
    {
        sw_timer_create(&timers[index]);
    }

    ASSERT_PROBE_EXPECT(sw_timer_create(&timers[SW_TIMER_MAX_TIMERS - 1U]), "false");
}
