#include "ott_animation.h"

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
#include "systick_bsp.h"
#include "user_button.h"

/* ==========================================================================
 * ott_animation - private
 * ========================================================================= */

#define OTT_ANIMATION_MS_PER_SECOND           (1000U)
#define OTT_ANIMATION_MS_PER_SECOND_F         (1000.0)
#define OTT_ANIMATION_PERCENT                 (100U)

/* A Pacman actor is one 16 x 16 sprite; five of them move on a game frame. */
#define OTT_ANIMATION_ACTOR_SIZE              (16)
#define OTT_ANIMATION_ACTOR_COUNT             (5U)

/* Constant across the whole ladder — that is the point. Roughly one sprite width
 * every eleven frames at 60 FPS, which is a brisk but readable Pacman speed. */
#define OTT_ANIMATION_SPEED_PIXELS_PER_SECOND (90)

/* Positions are carried in sixteenths of a pixel so that a rate change alters the
 * step, not the speed: at 60 FPS the step is 1.5 px, which an integer cannot hold. */
#define OTT_ANIMATION_SUBPIXEL_SHIFT          (4)
#define OTT_ANIMATION_SUBPIXEL_ONE            (1 << OTT_ANIMATION_SUBPIXEL_SHIFT)

#define OTT_ANIMATION_LANE_SPACING            (48)
#define OTT_ANIMATION_LANE_FIRST_Y            (32)

/* The dots the actors run over. Redrawing them behind a sprite is what makes this
 * the same work a real frame does, rather than a square on a blank field. */
#define OTT_ANIMATION_PELLET_SPACING          (16)
#define OTT_ANIMATION_PELLET_SIZE             (3)

#define OTT_ANIMATION_PASS_MS                 (3000U)
#define OTT_ANIMATION_TIMEOUT_MS              (100000U)

/* The rates worth judging: three steps below the target, then the target itself. Nothing
 * above it — the panel refreshes at 60 Hz, so a faster frame is overwritten unseen. */
static const uint16_t g_frame_rates[] = {10U, 15U, 30U, 60U};

#define OTT_ANIMATION_RATE_COUNT             (sizeof(g_frame_rates) / sizeof(g_frame_rates[0]))

/* A pass that misses its own pace is measuring the panel, not the eye, so its
 * verdict about smoothness would mean nothing. */
#define OTT_ANIMATION_RATE_TOLERANCE_PERCENT (95U)

#define OTT_ANIMATION_TARGET_RATE_INDEX      (3U) /* 60 FPS — the chosen frame rate */

/* Long enough for a stable average, short enough not to test anyone's patience. */
#define OTT_ANIMATION_CEILING_MS             (2000U)

typedef struct
{
    int32_t x_subpixel;
    int16_t y;
    framebuffer_color_t colour;
} ott_animation_actor_t;

static ott_animation_actor_t g_actors[OTT_ANIMATION_ACTOR_COUNT];

static const framebuffer_color_t g_actor_colours[OTT_ANIMATION_ACTOR_COUNT] = {
    FRAMEBUFFER_COLOR_YELLOW,          /* Pacman */
    FRAMEBUFFER_COLOR_RED,             /* Blinky */
    FRAMEBUFFER_RGB(255U, 184U, 255U), /* Pinky  */
    FRAMEBUFFER_COLOR_CYAN,            /* Inky   */
    FRAMEBUFFER_RGB(255U, 184U, 82U),  /* Clyde  */
};

static int16_t prv_get_pixel_x(const ott_animation_actor_t* const in_actor)
{
    return (int16_t)(in_actor->x_subpixel >> OTT_ANIMATION_SUBPIXEL_SHIFT);
}

/* Repaints the field behind a sprite: black, plus whichever pellets fall inside the
 * rectangle. A game redraws the maze cell the same way before drawing the actor. */
static void prv_draw_background(int16_t in_x, int16_t in_y, int16_t in_width)
{
    framebuffer_t* const framebuffer = ott_framebuffer_get();
    const int16_t pellet_y = (int16_t)(in_y + (OTT_ANIMATION_ACTOR_SIZE / 2) - (OTT_ANIMATION_PELLET_SIZE / 2));

    gfx_filled_rectangle(framebuffer, in_x, in_y, in_width, OTT_ANIMATION_ACTOR_SIZE, FRAMEBUFFER_COLOR_BLACK);

    for (int16_t pellet_x = (int16_t)((in_x / OTT_ANIMATION_PELLET_SPACING) * OTT_ANIMATION_PELLET_SPACING);
         pellet_x < (int16_t)(in_x + in_width); pellet_x = (int16_t)(pellet_x + OTT_ANIMATION_PELLET_SPACING))
    {
        gfx_filled_rectangle(framebuffer, pellet_x, pellet_y, OTT_ANIMATION_PELLET_SIZE, OTT_ANIMATION_PELLET_SIZE,
                             FRAMEBUFFER_COLOR_WHITE);
    }
}

static void prv_draw_actor(const ott_animation_actor_t* const in_actor)
{
    const int16_t x = prv_get_pixel_x(in_actor);

    gfx_filled_circle(ott_framebuffer_get(), (int16_t)(x + (OTT_ANIMATION_ACTOR_SIZE / 2)),
                      (int16_t)(in_actor->y + (OTT_ANIMATION_ACTOR_SIZE / 2)), OTT_ANIMATION_ACTOR_SIZE / 2,
                      in_actor->colour);
}

/* An actor is half off the screen while it enters and leaves, and the display port
 * refuses a rectangle that crosses the edge, so the caller clips. */
static void prv_present_clipped(int16_t in_x, int16_t in_y, int16_t in_width)
{
    const int16_t left = (in_x < 0) ? 0 : in_x;
    const int16_t unclipped_right = (int16_t)(in_x + in_width);
    const int16_t right = (unclipped_right > (int16_t)FRAMEBUFFER_WIDTH) ? (int16_t)FRAMEBUFFER_WIDTH : unclipped_right;

    if (right <= left)
    {
        return;
    }

    display_present_region(ott_framebuffer_get(), left, in_y, (int16_t)(right - left), OTT_ANIMATION_ACTOR_SIZE);
}

/* Sends the sprite's old and new footprint. While they overlap that is one
 * rectangle, which is both cheaper and free of the seam two adjacent transfers can
 * leave; once the step exceeds the sprite they are separate. */
static void prv_present_move(int16_t in_old_x, int16_t in_new_x, int16_t in_y)
{
    const int16_t left = (in_old_x < in_new_x) ? in_old_x : in_new_x;
    const int16_t right = (in_old_x < in_new_x) ? in_new_x : in_old_x;
    const int16_t span = (int16_t)((right - left) + OTT_ANIMATION_ACTOR_SIZE);

    if (span <= (2 * OTT_ANIMATION_ACTOR_SIZE))
    {
        prv_present_clipped(left, in_y, span);
    }
    else
    {
        prv_present_clipped(in_old_x, in_y, OTT_ANIMATION_ACTOR_SIZE);
        prv_present_clipped(in_new_x, in_y, OTT_ANIMATION_ACTOR_SIZE);
    }
}

/* One game frame: every actor vacates a footprint and enters another. */
static void prv_draw_frame(int32_t in_step_subpixel)
{
    for (size_t index = 0U; index < OTT_ANIMATION_ACTOR_COUNT; ++index)
    {
        ott_animation_actor_t* const actor = &g_actors[index];
        const int16_t old_x = prv_get_pixel_x(actor);
        int16_t new_x;

        actor->x_subpixel += in_step_subpixel;

        if (prv_get_pixel_x(actor) > (int16_t)FRAMEBUFFER_WIDTH)
        {
            actor->x_subpixel = -(OTT_ANIMATION_ACTOR_SIZE * OTT_ANIMATION_SUBPIXEL_ONE);
        }

        new_x = prv_get_pixel_x(actor);

        if (new_x == old_x)
        {
            continue;
        }

        prv_draw_background(old_x, actor->y, OTT_ANIMATION_ACTOR_SIZE);
        prv_draw_actor(actor);
        prv_present_move(old_x, new_x, actor->y);
    }
}

static void prv_reset_actors(void)
{
    for (size_t index = 0U; index < OTT_ANIMATION_ACTOR_COUNT; ++index)
    {
        g_actors[index].x_subpixel = (int32_t)(index * OTT_ANIMATION_ACTOR_SIZE * 2) * OTT_ANIMATION_SUBPIXEL_ONE;
        g_actors[index].y = (int16_t)(OTT_ANIMATION_LANE_FIRST_Y + (index * OTT_ANIMATION_LANE_SPACING));
        g_actors[index].colour = g_actor_colours[index];
    }
}

static void prv_draw_field(void)
{
    framebuffer_t* const framebuffer = ott_framebuffer_get();

    framebuffer_fill(framebuffer, FRAMEBUFFER_COLOR_BLACK);

    for (size_t index = 0U; index < OTT_ANIMATION_ACTOR_COUNT; ++index)
    {
        prv_draw_background(0, g_actors[index].y, (int16_t)FRAMEBUFFER_WIDTH);
        prv_draw_actor(&g_actors[index]);
    }

    display_present(framebuffer);
}

/* Runs frames at a fixed pace until the deadline, and reports what it managed.
 *
 * The deadline of frame n is computed from the pass start rather than from the last
 * frame, so a frame that runs long is absorbed instead of pushing every later frame
 * back — the pace of the ladder has to be the rate it claims.
 */
static uint32_t prv_run_pass(uint16_t in_frames_per_second, bool in_is_paced, uint32_t in_duration_ms,
                             uint32_t* out_busy_ms)
{
    const int32_t step_subpixel =
        ((int32_t)OTT_ANIMATION_SPEED_PIXELS_PER_SECOND * OTT_ANIMATION_SUBPIXEL_ONE) / (int32_t)in_frames_per_second;
    const uint32_t start_tick = systick_bsp_get_tick();
    uint32_t frame_count = 0U;
    uint32_t busy_ms = 0U;
    uint32_t elapsed_ms;

    while ((systick_bsp_get_tick() - start_tick) < in_duration_ms)
    {
        const uint32_t deadline = start_tick + ((frame_count * OTT_ANIMATION_MS_PER_SECOND) / in_frames_per_second);
        uint32_t draw_start_tick;

        while (in_is_paced && ((int32_t)(systick_bsp_get_tick() - deadline) < 0))
        {
            /* Waiting out the frame period: this is the headroom a game would use. */
        }

        draw_start_tick = systick_bsp_get_tick();

        prv_draw_frame(step_subpixel);

        busy_ms += systick_bsp_get_tick() - draw_start_tick;
        ++frame_count;
    }

    elapsed_ms = systick_bsp_get_tick() - start_tick;

    *out_busy_ms = busy_ms;

    return (frame_count * OTT_ANIMATION_MS_PER_SECOND) / elapsed_ms;
}

/* Prints the pass as a rate plus the two numbers that explain it: how far a sprite
 * jumps between frames, and how much of the frame the drawing actually took. */
static void prv_report_pass(uint16_t in_requested_fps, uint32_t in_achieved_fps, uint32_t in_busy_ms,
                            uint32_t in_duration_ms)
{
    const double step_pixels = (double)OTT_ANIMATION_SPEED_PIXELS_PER_SECOND / (double)in_requested_fps;
    const uint32_t frame_count = (in_achieved_fps * in_duration_ms) / OTT_ANIMATION_MS_PER_SECOND;
    const double busy_per_frame = (frame_count > 0U) ? ((double)in_busy_ms / (double)frame_count) : 0.0;
    const double period_ms = OTT_ANIMATION_MS_PER_SECOND_F / (double)in_requested_fps;

    cli_print("    asked %u FPS, held %lu FPS — %d.%02d px per frame, %d.%02d of %d.%02d ms used (%u %%)",
              (unsigned)in_requested_fps, (unsigned long)in_achieved_fps, (int)step_pixels,
              (int)((step_pixels - (int)step_pixels) * 100.0), (int)busy_per_frame,
              (int)((busy_per_frame - (int)busy_per_frame) * 100.0), (int)period_ms,
              (int)((period_ms - (int)period_ms) * 100.0),
              (unsigned)((busy_per_frame * OTT_ANIMATION_PERCENT) / period_ms));
}

/* The same frames as the target pass, but with the pacing removed: what the render
 * path manages when nothing holds it back.
 *
 * The ladder shows that 60 FPS is met; this shows by how much, which is the number
 * M3 needs. The step stays the one belonging to the target rate — dividing the step
 * by an imagined rate instead would shrink the sprite's movement until it stopped
 * redrawing anything, and measure nothing at all. */
static void prv_measure_ceiling(void)
{
    const uint16_t target_fps = g_frame_rates[OTT_ANIMATION_TARGET_RATE_INDEX];
    uint32_t busy_ms = 0U;
    uint32_t ceiling_fps;

    cli_print("  unpaced, same frames: how fast the render path goes when nothing holds it");

    ceiling_fps = prv_run_pass(target_fps, false, OTT_ANIMATION_CEILING_MS, &busy_ms);

    cli_print("    %lu FPS — %lu times the %u FPS target, so %u %% of the frame is the game's",
              (unsigned long)ceiling_fps, (unsigned long)(ceiling_fps / target_fps), (unsigned)target_fps,
              (unsigned)(OTT_ANIMATION_PERCENT - ((target_fps * OTT_ANIMATION_PERCENT) / ceiling_fps)));
}

/* The operator drives the rate here, because the answer to "is this smooth" is
 * easiest to see by going back and forth across the point where it stops being so. */
static bool prv_run_interactive(void)
{
    size_t rate_index = OTT_ANIMATION_TARGET_RATE_INDEX;
    const uint32_t start_tick = systick_bsp_get_tick();
    bool has_confirmed = false;
    uint32_t busy_ms;

    cli_print("Now you steer the rate: NORTH raises it, SOUTH lowers it, B1 ends the test.");
    cli_print("The actors keep the same speed at every rate — only the smoothness changes.");
    cli_print("  %u FPS", (unsigned)g_frame_rates[rate_index]);

    while (!has_confirmed && ((systick_bsp_get_tick() - start_tick) < OTT_ANIMATION_TIMEOUT_MS))
    {
        /* Short slices, so a key press is answered without waiting out a whole pass. */
        (void)prv_run_pass(g_frame_rates[rate_index], true, OTT_ANIMATION_MS_PER_SECOND / 4U, &busy_ms);

        if (joystick_take_press(JOYSTICK_KEY_NORTH) && (rate_index < (OTT_ANIMATION_RATE_COUNT - 1U)))
        {
            ++rate_index;
            cli_print("  %u FPS", (unsigned)g_frame_rates[rate_index]);
        }

        if (joystick_take_press(JOYSTICK_KEY_SOUTH) && (rate_index > 0U))
        {
            --rate_index;
            cli_print("  %u FPS", (unsigned)g_frame_rates[rate_index]);
        }

        has_confirmed = user_button_take_press();
    }

    return has_confirmed;
}

/* ==========================================================================
 * ott_animation - public
 * ========================================================================= */

bool ott_animation_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    bool has_missed_a_rate = false;
    bool has_confirmed;

    (void)in_parameter;

    cli_print("Animation: five actors at a constant %d px/s, the frame rate varying under them.",
              OTT_ANIMATION_SPEED_PIXELS_PER_SECOND);

    display_init();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    prv_reset_actors();
    prv_draw_field();

    for (size_t index = 0U; index < OTT_ANIMATION_RATE_COUNT; ++index)
    {
        const uint16_t requested_fps = g_frame_rates[index];
        uint32_t busy_ms = 0U;
        uint32_t achieved_fps;
        bool is_rate_held;

        cli_print("  %u FPS for %lu s — watch the actors, not the console", (unsigned)requested_fps,
                  (unsigned long)(OTT_ANIMATION_PASS_MS / OTT_ANIMATION_MS_PER_SECOND));

        achieved_fps = prv_run_pass(requested_fps, true, OTT_ANIMATION_PASS_MS, &busy_ms);

        prv_report_pass(requested_fps, achieved_fps, busy_ms, OTT_ANIMATION_PASS_MS);

        is_rate_held = ((achieved_fps * OTT_ANIMATION_PERCENT)
                        >= ((uint32_t)requested_fps * OTT_ANIMATION_RATE_TOLERANCE_PERCENT));

        if (!is_rate_held)
        {
            has_missed_a_rate = true;

            cli_print("    that pass did not hold its pace — its smoothness proves nothing");
        }
    }

    prv_measure_ceiling();

    has_confirmed = prv_run_interactive();

    if (!has_confirmed)
    {
        (void)snprintf(out_reason, in_reason_size, "not confirmed at the board within %u s",
                       OTT_ANIMATION_TIMEOUT_MS / OTT_ANIMATION_MS_PER_SECOND);
    }
    else if (has_missed_a_rate)
    {
        (void)snprintf(out_reason, in_reason_size, "at least one pass fell short of the rate it asked for");
    }
    else
    {
        /* Every pace was held, and the motion was judged at the board. */
    }

    return has_confirmed && !has_missed_a_rate;
}
