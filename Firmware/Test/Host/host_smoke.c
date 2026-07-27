/*
 * Host smoke test — proves the host build actually runs, not just links.
 *
 * PLACEHOLDER. This is not the unit-test suite: VT-UNIT-001..005 need Ceedling with
 * CMock so a module can be tested against mocked dependencies (RF-002), which needs
 * Ruby. Until that lands, this exercises the modules that are already
 * hardware-independent so the host path cannot rot unnoticed. Delete it once real
 * tests cover the same ground.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "delay.h"
#include "retain_ram.h"
#include "sw_timer.h"
#include "systick_bsp.h"

/* ==========================================================================
 * host_smoke - private
 * ========================================================================= */

#define HOST_SMOKE_DELAY_MS (50U)
#define HOST_SMOKE_TIMER_MS (20U)
#define HOST_SMOKE_PERIODIC_REARM_COUNT (3U)

/* Upper bound on how long a check may wait before it is called a failure, so a
 * broken timer fails the run instead of hanging it. */
#define HOST_SMOKE_GUARD_MS (2000U)

static uint32_t g_failures = 0U;
static uint32_t g_one_shot_calls = 0U;
static uint32_t g_periodic_calls = 0U;
static sw_timer_t g_one_shot_timer;
static sw_timer_t g_periodic_timer;

static void prv_check(bool in_condition, const char* const in_what)
{
    if (in_condition)
    {
        (void)printf("  ok   %s\n", in_what);

        return;
    }

    (void)printf("  FAIL %s\n", in_what);
    ++g_failures;
}

static void prv_on_one_shot(void)
{
    ++g_one_shot_calls;
}

static void prv_on_periodic(void)
{
    ++g_periodic_calls;

    /* Re-arming from the callback is what makes a one-shot timer periodic. */
    if (g_periodic_calls < HOST_SMOKE_PERIODIC_REARM_COUNT)
    {
        sw_timer_start(&g_periodic_timer, HOST_SMOKE_TIMER_MS, prv_on_periodic);
    }
}

/* Pump sw_timer until the guard expires. */
static void prv_pump(uint32_t in_duration_ms)
{
    const uint32_t start_tick = systick_bsp_get_tick();

    while ((uint32_t)(systick_bsp_get_tick() - start_tick) < in_duration_ms)
    {
        sw_timer_process();
    }
}

static void prv_test_tick_advances(void)
{
    const uint32_t before = systick_bsp_get_tick();

    delay_ms(HOST_SMOKE_DELAY_MS);

    prv_check((uint32_t)(systick_bsp_get_tick() - before) >= HOST_SMOKE_DELAY_MS,
              "delay_ms waits at least the requested time");
}

static void prv_test_one_shot_fires_once(void)
{
    sw_timer_create(&g_one_shot_timer);
    sw_timer_start(&g_one_shot_timer, HOST_SMOKE_TIMER_MS, prv_on_one_shot);

    prv_check(sw_timer_is_active(&g_one_shot_timer), "an armed timer reports active");

    prv_pump(HOST_SMOKE_GUARD_MS);

    prv_check(g_one_shot_calls == 1U, "a one-shot timer fires exactly once");
    prv_check(!sw_timer_is_active(&g_one_shot_timer), "an expired timer reports inactive");
}

static void prv_test_periodic_rearms(void)
{
    sw_timer_create(&g_periodic_timer);
    sw_timer_start(&g_periodic_timer, HOST_SMOKE_TIMER_MS, prv_on_periodic);

    prv_pump(HOST_SMOKE_GUARD_MS);

    prv_check(g_periodic_calls == HOST_SMOKE_PERIODIC_REARM_COUNT,
              "a callback that re-arms its timer makes it periodic");
}

static void prv_test_stop_prevents_firing(void)
{
    static sw_timer_t stopped_timer;
    const uint32_t calls_before = g_one_shot_calls;

    sw_timer_create(&stopped_timer);
    sw_timer_start(&stopped_timer, HOST_SMOKE_TIMER_MS, prv_on_one_shot);
    sw_timer_stop(&stopped_timer);

    prv_pump(HOST_SMOKE_TIMER_MS * 4U);

    prv_check(g_one_shot_calls == calls_before, "a stopped timer never fires");
}

static void prv_test_retained_buffer_round_trip(void)
{
    uint8_t written[RETAIN_RAM_BUFFER_SIZE];
    uint8_t read_back[RETAIN_RAM_BUFFER_SIZE];

    for (size_t index = 0U; index < sizeof(written); ++index)
    {
        written[index] = (uint8_t)index;
    }

    memset(read_back, 0, sizeof(read_back));

    retained_ram_write(written, sizeof(written));
    retained_ram_read(read_back, sizeof(read_back));

    prv_check(memcmp(written, read_back, sizeof(written)) == 0,
              "the retained buffer round-trips its contents");
}

/* ==========================================================================
 * host_smoke - entry point
 * ========================================================================= */

int main(void)
{
    (void)printf("host smoke test\n");

    systick_bsp_init();
    sw_timer_init();

    prv_test_tick_advances();
    prv_test_one_shot_fires_once();
    prv_test_periodic_rearms();
    prv_test_stop_prevents_firing();
    prv_test_retained_buffer_round_trip();

    if (g_failures == 0U)
    {
        (void)printf("PASS\n");

        return 0;
    }

    (void)printf("FAIL (%u check(s))\n", (unsigned)g_failures);

    return 1;
}
