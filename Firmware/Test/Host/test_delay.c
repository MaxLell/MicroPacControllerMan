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
    systick_bsp_get_tick_ExpectAndReturn(10U);
    systick_bsp_get_tick_ExpectAndReturn(10U); /*  0 elapsed -> keep waiting */
    systick_bsp_get_tick_ExpectAndReturn(12U); /*  2 elapsed -> keep waiting */
    systick_bsp_get_tick_ExpectAndReturn(15U); /*  5 elapsed -> done         */

    delay_ms(5U);
}

void test_delay_ms_returns_immediately_for_a_zero_delay(void)
{
    systick_bsp_get_tick_ExpectAndReturn(42U);
    systick_bsp_get_tick_ExpectAndReturn(42U);

    delay_ms(0U);
}

void test_delay_ms_handles_tick_wraparound(void)
{
    /* Starting just below the wrap: unsigned subtraction has to keep working across
     * it, otherwise the delay would return instantly or hang for 49 days. */
    systick_bsp_get_tick_ExpectAndReturn(UINT32_MAX - 1U);
    systick_bsp_get_tick_ExpectAndReturn(UINT32_MAX); /* 1 elapsed -> keep waiting */
    systick_bsp_get_tick_ExpectAndReturn(1U);         /* 3 elapsed -> done          */

    delay_ms(3U);
}

void test_delay_ms_does_not_return_early_when_the_tick_stands_still(void)
{
    systick_bsp_get_tick_ExpectAndReturn(7U);
    systick_bsp_get_tick_ExpectAndReturn(7U);
    systick_bsp_get_tick_ExpectAndReturn(7U);
    systick_bsp_get_tick_ExpectAndReturn(7U);
    systick_bsp_get_tick_ExpectAndReturn(9U); /* 2 elapsed -> done */

    delay_ms(2U);
}
