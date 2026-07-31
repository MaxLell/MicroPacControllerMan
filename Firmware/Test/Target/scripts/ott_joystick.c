#include "ott_joystick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "dio_bsp.h"
#include "sw_timer.h"

/* ==========================================================================
 * ott_joystick - private
 * ========================================================================= */

/* Diagnostic cap, so the board returns to nominal mode even if nothing happens. */
#define OTT_JOYSTICK_TIMEOUT_MS (60000U)
#define OTT_JOYSTICK_MS_PER_SECOND (1000U)

/* A key held by a human is stable for far longer than this; the count only has to
 * outlast contact bounce. */
#define OTT_JOYSTICK_STABLE_SAMPLES (2000U)

typedef struct
{
    dio_bsp_pin_e pin;
    const char* name;
} ott_joystick_key_t;

/* The keys are read straight through dio_bsp rather than through a debounced
 * abstraction: what this test exists to check is the pin map itself. */
static const ott_joystick_key_t g_keys[] = {
    {DIO_BSP_PIN_JOYSTICK_NORTH, "NORTH"},   {DIO_BSP_PIN_JOYSTICK_EAST, "EAST"},
    {DIO_BSP_PIN_JOYSTICK_SOUTH, "SOUTH"},   {DIO_BSP_PIN_JOYSTICK_WEST, "WEST"},
    {DIO_BSP_PIN_JOYSTICK_CENTER, "CENTER"},
};

#define OTT_JOYSTICK_KEY_COUNT (sizeof(g_keys) / sizeof(g_keys[0]))

#define OTT_JOYSTICK_HEARTBEAT_PERIOD_MS (1000U)

static sw_timer_t g_timeout_timer;
static sw_timer_t g_heartbeat_timer;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

/* Streams the raw level of all five keys, so a key that never registers can be told
 * apart from a key whose pin never moves. */
static void prv_on_heartbeat(void)
{
    char line[80];
    size_t length = 0U;

    for (size_t index = 0U; index < OTT_JOYSTICK_KEY_COUNT; ++index)
    {
        length += (size_t)snprintf(&line[length], sizeof(line) - length, "%s=%u ",
                                   g_keys[index].name,
                                   (dio_bsp_get_pin(g_keys[index].pin) == DIO_BSP_PIN_STATE_LOW)
                                       ? 1U
                                       : 0U);
    }

    cli_print("JOY %s (1 = pressed)", line);

    sw_timer_start(&g_heartbeat_timer, OTT_JOYSTICK_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);
}

/* The keys are active low and the shield pulls them up, so LOW means pressed. */
static bool prv_is_pressed(dio_bsp_pin_e in_pin)
{
    return dio_bsp_get_pin(in_pin) == DIO_BSP_PIN_STATE_LOW;
}

static void prv_report_missing(char* out_reason, size_t in_reason_size, const bool* in_is_seen)
{
    char missing[64] = {'\0'};
    size_t length = 0U;

    for (size_t index = 0U; index < OTT_JOYSTICK_KEY_COUNT; ++index)
    {
        if (!in_is_seen[index])
        {
            length += (size_t)snprintf(&missing[length], sizeof(missing) - length, "%s%s",
                                       (length > 0U) ? "," : "", g_keys[index].name);
        }
    }

    (void)snprintf(out_reason, in_reason_size, "keys never seen: %s", missing);
}

/* ==========================================================================
 * ott_joystick - public
 * ========================================================================= */

bool ott_joystick_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool is_seen[OTT_JOYSTICK_KEY_COUNT] = {false};
    uint32_t stable_count[OTT_JOYSTICK_KEY_COUNT] = {0U};
    uint32_t seen_count = 0U;
    bool has_passed;

    (void)in_parameter;

    sw_timer_create(&g_timeout_timer);
    sw_timer_create(&g_heartbeat_timer);

    cli_print("Joystick test: press each of the five keys once, in any order.");
    cli_print("Watch the name the firmware prints — it must match the key you pressed.");
    cli_print("Times out after %u s.", OTT_JOYSTICK_TIMEOUT_MS / OTT_JOYSTICK_MS_PER_SECOND);

    sw_timer_start(&g_timeout_timer, OTT_JOYSTICK_TIMEOUT_MS, prv_on_timeout);
    sw_timer_start(&g_heartbeat_timer, OTT_JOYSTICK_HEARTBEAT_PERIOD_MS, prv_on_heartbeat);

    while (sw_timer_is_active(&g_timeout_timer) && (seen_count < OTT_JOYSTICK_KEY_COUNT))
    {
        sw_timer_process();

        for (size_t index = 0U; index < OTT_JOYSTICK_KEY_COUNT; ++index)
        {
            if (!prv_is_pressed(g_keys[index].pin))
            {
                stable_count[index] = 0U;

                continue;
            }

            ++stable_count[index];

            if ((stable_count[index] == OTT_JOYSTICK_STABLE_SAMPLES) && !is_seen[index])
            {
                is_seen[index] = true;
                ++seen_count;

                cli_print("  %s pressed  (%lu/%u)", g_keys[index].name, (unsigned long)seen_count,
                          (unsigned)OTT_JOYSTICK_KEY_COUNT);
            }
        }
    }

    sw_timer_stop(&g_timeout_timer);
    sw_timer_stop(&g_heartbeat_timer);

    has_passed = (seen_count >= OTT_JOYSTICK_KEY_COUNT);

    if (!has_passed)
    {
        prv_report_missing(out_reason, in_reason_size, is_seen);
    }

    return has_passed;
}
