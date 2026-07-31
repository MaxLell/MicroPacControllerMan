#include "ott_joystick_dot.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Cli.h"
#include "display.h"
#include "framebuffer.h"
#include "gfx.h"
#include "joystick.h"
#include "ott_framebuffer.h"
#include "st7789.h"
#include "sw_timer.h"
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_joystick_dot - private
 * ========================================================================= */

#define OTT_JOYSTICK_DOT_TIMEOUT_MS      (120000U)
#define OTT_JOYSTICK_DOT_MS_PER_SECOND   (1000U)
#define OTT_JOYSTICK_DOT_MS_PER_SECOND_F (1000.0)

/* Big enough to be unmistakable at the panel's 0.14 mm pixel pitch; the grid step is
 * the same, so one press moves the dot exactly its own width. */
#define OTT_JOYSTICK_DOT_SIZE            (16)

#define OTT_JOYSTICK_DOT_BORDER          (2)

#define OTT_JOYSTICK_DOT_COLUMNS         (FRAMEBUFFER_WIDTH / OTT_JOYSTICK_DOT_SIZE)
#define OTT_JOYSTICK_DOT_ROWS            (FRAMEBUFFER_HEIGHT / OTT_JOYSTICK_DOT_SIZE)

/* Enough steps to average out the tick's 1 ms granularity. */
#define OTT_JOYSTICK_DOT_TIMED_STEPS     (100U)

typedef struct
{
    int16_t column;
    int16_t row;
} ott_joystick_dot_cell_t;

static sw_timer_t g_timeout_timer;

static ott_joystick_dot_cell_t g_cell;

static void prv_on_timeout(void)
{
    /* Nothing to do: the run loop watches sw_timer_is_active(). */
}

/* Draws the dot into the shared frame buffer and sends only its cell. Erasing the
 * cell it left and painting the one it entered is what a game frame does, so the
 * cost measured here is the cost of a move, not of a screen. */
static void prv_paint_cell(const ott_joystick_dot_cell_t* const in_cell, bool in_is_dot)
{
    const int16_t x = (int16_t)(in_cell->column * OTT_JOYSTICK_DOT_SIZE);
    const int16_t y = (int16_t)(in_cell->row * OTT_JOYSTICK_DOT_SIZE);

    gfx_filled_rectangle(ott_framebuffer_get(), x, y, OTT_JOYSTICK_DOT_SIZE, OTT_JOYSTICK_DOT_SIZE,
                         in_is_dot ? FRAMEBUFFER_COLOR_YELLOW : FRAMEBUFFER_COLOR_BLACK);

    display_present_region(ott_framebuffer_get(), x, y, OTT_JOYSTICK_DOT_SIZE, OTT_JOYSTICK_DOT_SIZE);
}

static void prv_move_to(const ott_joystick_dot_cell_t* const in_target)
{
    prv_paint_cell(&g_cell, false);

    g_cell = *in_target;

    prv_paint_cell(&g_cell, true);
}

/* The dot stops at the border rather than wrapping, so an operator can tell a key
 * that does nothing from a dot that has run out of screen. */
static bool prv_clamp(ott_joystick_dot_cell_t* inout_cell)
{
    const int16_t first = OTT_JOYSTICK_DOT_BORDER;
    const int16_t last_column = (int16_t)(OTT_JOYSTICK_DOT_COLUMNS - 1 - OTT_JOYSTICK_DOT_BORDER);
    const int16_t last_row = (int16_t)(OTT_JOYSTICK_DOT_ROWS - 1 - OTT_JOYSTICK_DOT_BORDER);
    const ott_joystick_dot_cell_t before = *inout_cell;

    if (inout_cell->column < first)
    {
        inout_cell->column = first;
    }
    else if (inout_cell->column > last_column)
    {
        inout_cell->column = last_column;
    }
    else
    {
        /* Inside horizontally. */
    }

    if (inout_cell->row < first)
    {
        inout_cell->row = first;
    }
    else if (inout_cell->row > last_row)
    {
        inout_cell->row = last_row;
    }
    else
    {
        /* Inside vertically. */
    }

    return (inout_cell->column != before.column) || (inout_cell->row != before.row);
}

static ott_joystick_dot_cell_t prv_get_center_cell(void)
{
    const ott_joystick_dot_cell_t center = {(int16_t)(OTT_JOYSTICK_DOT_COLUMNS / 2),
                                            (int16_t)(OTT_JOYSTICK_DOT_ROWS / 2)};

    return center;
}

/* Black playfield inside a blue frame: the frame names the edge the dot stops at, so
 * "the dot will not go further" and "the key stopped working" look different. */
static void prv_draw_playfield(void)
{
    framebuffer_t* const framebuffer = ott_framebuffer_get();
    const int16_t inset = (int16_t)((OTT_JOYSTICK_DOT_BORDER * OTT_JOYSTICK_DOT_SIZE) - OTT_JOYSTICK_DOT_SIZE);

    framebuffer_fill(framebuffer, FRAMEBUFFER_COLOR_BLACK);
    gfx_rectangle(framebuffer, inset, inset, (int16_t)(FRAMEBUFFER_WIDTH - (2 * inset)),
                  (int16_t)(FRAMEBUFFER_HEIGHT - (2 * inset)), FRAMEBUFFER_COLOR_BLUE);

    display_present(framebuffer);
}

/* One move is two regions — the cell vacated and the cell entered — and that is what
 * the input latency of NFR-003 has to fit into, on top of the debounce. */
static void prv_measure_step_cost(void)
{
    const ott_joystick_dot_cell_t start = g_cell;
    uint32_t start_tick;
    uint32_t elapsed_ms;
    double milliseconds_per_step;

    cli_print("  timing %lu moves — the dot sweeps its row, then returns to the middle",
              (unsigned long)OTT_JOYSTICK_DOT_TIMED_STEPS);

    start_tick = systick_bsp_get_tick();

    for (uint32_t step = 0U; step < OTT_JOYSTICK_DOT_TIMED_STEPS; ++step)
    {
        ott_joystick_dot_cell_t target = g_cell;

        target.column = (int16_t)(OTT_JOYSTICK_DOT_BORDER
                                  + (step % (uint32_t)(OTT_JOYSTICK_DOT_COLUMNS - (2 * OTT_JOYSTICK_DOT_BORDER))));

        prv_move_to(&target);
    }

    elapsed_ms = systick_bsp_get_tick() - start_tick;
    milliseconds_per_step = (double)elapsed_ms / OTT_JOYSTICK_DOT_TIMED_STEPS;

    cli_print("  %lu moves in %lu ms -> %d.%02d ms per move", (unsigned long)OTT_JOYSTICK_DOT_TIMED_STEPS,
              (unsigned long)elapsed_ms, (int)milliseconds_per_step,
              (int)((milliseconds_per_step - (int)milliseconds_per_step) * 100.0));
    cli_print("  NFR-003 allows 30 ms: %u ms of it goes on debouncing the key, %d.%02d ms on drawing.",
              (unsigned)JOYSTICK_DEBOUNCE_MS, (int)milliseconds_per_step,
              (int)((milliseconds_per_step - (int)milliseconds_per_step) * 100.0));

    prv_move_to(&start);
}

/* A direction key moves the dot one cell; CENTER puts it back in the middle. */
static bool prv_apply_key(joystick_key_e in_key, ott_joystick_dot_cell_t* out_target)
{
    *out_target = g_cell;

    switch (in_key)
    {
        case JOYSTICK_KEY_NORTH: --out_target->row; break;

        case JOYSTICK_KEY_SOUTH: ++out_target->row; break;

        case JOYSTICK_KEY_WEST: --out_target->column; break;

        case JOYSTICK_KEY_EAST: ++out_target->column; break;

        case JOYSTICK_KEY_CENTER: *out_target = prv_get_center_cell(); break;

        default: return false;
    }

    return true;
}

static void prv_report_missing(char* out_reason, size_t in_reason_size, const bool* in_is_seen)
{
    char missing[64] = {'\0'};
    size_t length = 0U;

    for (size_t index = 0U; index < (size_t)JOYSTICK_KEY_COUNT; ++index)
    {
        if (!in_is_seen[index])
        {
            length += (size_t)snprintf(&missing[length], sizeof(missing) - length, "%s%s", (length > 0U) ? "," : "",
                                       joystick_get_key_name((joystick_key_e)index));
        }
    }

    (void)snprintf(out_reason, in_reason_size, "keys never used: %s", missing);
}

/* ==========================================================================
 * ott_joystick_dot - public
 * ========================================================================= */

bool ott_joystick_dot_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool is_seen[JOYSTICK_KEY_COUNT] = {false};
    size_t seen_count = 0U;
    bool has_confirmed = false;
    bool has_all_keys;

    (void)in_parameter;

    cli_print("Joystick dot: input and display together — the integration test for M2.");

    display_init();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    prv_draw_playfield();

    g_cell = prv_get_center_cell();
    prv_paint_cell(&g_cell, true);

    prv_measure_step_cost();

    cli_print("Push the joystick: the YELLOW dot must move the way you pushed — up is");
    cli_print("towards the top of the panel. CENTER puts it back in the middle, and the");
    cli_print("dot stops at the blue frame instead of wrapping.");
    cli_print("When all five keys have moved it correctly, press B1 to pass.");
    cli_print("Times out after %u s.", OTT_JOYSTICK_DOT_TIMEOUT_MS / OTT_JOYSTICK_DOT_MS_PER_SECOND);

    sw_timer_create(&g_timeout_timer);
    sw_timer_start(&g_timeout_timer, OTT_JOYSTICK_DOT_TIMEOUT_MS, prv_on_timeout);

    while (sw_timer_is_active(&g_timeout_timer) && !has_confirmed)
    {
        sw_timer_process();

        for (size_t index = 0U; index < (size_t)JOYSTICK_KEY_COUNT; ++index)
        {
            const joystick_key_e key = (joystick_key_e)index;
            ott_joystick_dot_cell_t target;

            if (!joystick_take_press(key))
            {
                continue;
            }

            if (!is_seen[index])
            {
                is_seen[index] = true;
                ++seen_count;
            }

            (void)prv_apply_key(key, &target);

            if (prv_clamp(&target))
            {
                cli_print("  %s — at the frame, the dot stays put", joystick_get_key_name(key));
            }
            else
            {
                cli_print("  %s -> cell %d,%d  (%u/%u keys used)", joystick_get_key_name(key), (int)target.column,
                          (int)target.row, (unsigned)seen_count, (unsigned)JOYSTICK_KEY_COUNT);
            }

            prv_move_to(&target);
        }

        has_confirmed = user_button_take_press();
    }

    sw_timer_stop(&g_timeout_timer);

    has_all_keys = (seen_count >= (size_t)JOYSTICK_KEY_COUNT);

    if (!has_confirmed)
    {
        (void)snprintf(out_reason, in_reason_size, "not confirmed at the board within %u s",
                       OTT_JOYSTICK_DOT_TIMEOUT_MS / OTT_JOYSTICK_DOT_MS_PER_SECOND);
    }
    else if (!has_all_keys)
    {
        prv_report_missing(out_reason, in_reason_size, is_seen);
    }
    else
    {
        /* Confirmed, and every key moved the dot. */
    }

    return has_confirmed && has_all_keys;
}
