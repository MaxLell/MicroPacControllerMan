/*
 * Unit tests for Services/delay.
 *
 * The tick source is mocked, so these run in microseconds and can exercise cases
 * real time cannot reach on demand — notably the 32-bit tick wrapping around.
 */
#include <stdint.h>

/* Not used directly — Ceedling picks the sources to link from the includes it sees
 * here, and every test executable links Test/support/assert_probe.c, which needs
 * custom_assert. Without this the link fails. */
#include "custom_assert.h"
#include "delay.h"
#include "mock_systick_bsp.h"
#include "unity.h"

/* A delay, and the tick values the mocked source reports while it runs. The tick starts
 * at an arbitrary non-zero value on purpose: a delay that mistakenly compared against
 * zero rather than the elapsed difference would pass with a start of 0. */
#define TEST_DELAY_MS           (5U)
#define TEST_START_TICK         (10U)
#define TEST_TICK_PART_WAY      (TEST_START_TICK + 2U)
#define TEST_TICK_DONE          (TEST_START_TICK + TEST_DELAY_MS)

#define TEST_ZERO_DELAY_MS      (0U)
#define TEST_ZERO_DELAY_TICK    (42U)

/* Starting one tick below the wrap, so the unsigned subtraction has to carry across it. */
#define TEST_WRAP_DELAY_MS      (3U)
#define TEST_WRAP_START_TICK    (UINT32_MAX - 1U)
#define TEST_WRAP_MIDPOINT_TICK (UINT32_MAX)
#define TEST_WRAP_DONE_TICK     (1U)

/* A tick that stands still for several polls before finally advancing. */
#define TEST_STALLED_DELAY_MS   (2U)
#define TEST_STALLED_TICK       (7U)
#define TEST_STALLED_DONE_TICK  (TEST_STALLED_TICK + TEST_STALLED_DELAY_MS)

void setUp(void)
{
}

void tearDown(void)
{
}

void test_delay_ms_waits_until_the_requested_time_has_elapsed(void)
{
    /* One call captures the start tick, then one per loop check until the elapsed
     * time reaches the requested delay. */
    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_START_TICK);    /* 0 elapsed -> waiting */
    systick_bsp_get_tick_ExpectAndReturn(TEST_TICK_PART_WAY); /* 2 elapsed -> waiting */
    systick_bsp_get_tick_ExpectAndReturn(TEST_TICK_DONE);     /* 5 elapsed -> done    */

    delay_ms(TEST_DELAY_MS);
}

void test_delay_ms_returns_immediately_for_a_zero_delay(void)
{
    systick_bsp_get_tick_ExpectAndReturn(TEST_ZERO_DELAY_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_ZERO_DELAY_TICK);

    delay_ms(TEST_ZERO_DELAY_MS);
}

void test_delay_ms_handles_tick_wraparound(void)
{
    /* Starting just below the wrap: unsigned subtraction has to keep working across
     * it, otherwise the delay would return instantly or hang for 49 days. */
    systick_bsp_get_tick_ExpectAndReturn(TEST_WRAP_START_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_WRAP_MIDPOINT_TICK); /* 1 elapsed -> waiting */
    systick_bsp_get_tick_ExpectAndReturn(TEST_WRAP_DONE_TICK);     /* 3 elapsed -> done    */

    delay_ms(TEST_WRAP_DELAY_MS);
}

void test_delay_ms_does_not_return_early_when_the_tick_stands_still(void)
{
    systick_bsp_get_tick_ExpectAndReturn(TEST_STALLED_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_STALLED_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_STALLED_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_STALLED_TICK);
    systick_bsp_get_tick_ExpectAndReturn(TEST_STALLED_DONE_TICK); /* 2 elapsed -> done */

    delay_ms(TEST_STALLED_DELAY_MS);
}
