#include "ott_ai_frame_cost.h"

#include <stdio.h>

#include "Cli.h"
#include "ai_weights.h"
#include "game_session.h"
#include "pacman_ai.h"
#include "st7789.h"
#include "sw_timer.h"
#include "systick_bsp.h"

/* ==========================================================================
 * ott_ai_frame_cost - private
 * ========================================================================= */

/*! \brief Frames measured. About five seconds of play, which is long enough to contain the
 *         expensive frames — a power pellet turning four ghosts blue, a life lost and the actors
 *         being put back — rather than only the cheap opening ones. */
#define OTT_AI_FRAME_COST_FRAMES     (300U)

#define OTT_AI_FRAME_COST_MS_PER_SEC (1000U)

/*! \brief Frames drawn before the measurement starts.
 *
 * The first frame of a level draws the whole playfield — every wall's geometry computed and the
 * entire panel written — and it was **measured at 955 ms**, which is nearly a second and swamps
 * everything after it. That is a real cost and it is reported separately, but it is not what "does
 * the game run smoothly" asks about: it happens once, behind the level-start pause, and no player
 * is steering through it. Ten frames is well clear of it. */
#define OTT_AI_FRAME_COST_WARMUP     (10U)

/*! \brief The rate the session has to hold, as a percentage of the one its frame period asks for.
 *
 * Not 100: the session paces itself off a 1 ms tick, so a frame that finishes just after a tick
 * boundary waits for the next one, and the achieved rate sits a little under the nominal one even
 * when nothing is late. Below this, frames are being missed rather than rounded. */
#define OTT_AI_FRAME_COST_MIN_RATE   (90U)

/* ==========================================================================
 * ott_ai_frame_cost - public
 * ========================================================================= */

bool ott_ai_frame_cost_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size)
{
    uint32_t busy_ms = 0U;
    uint32_t worst_ms = 0U;
    uint32_t frames = 0U;

    (void)in_parameter;

    cli_print("AI frame cost: what a frame costs with the agent deciding (NFR-006).");
    cli_print("weights %s", AI_WEIGHTS_DIGEST);

    game_session_init();

    if (!st7789_is_present())
    {
        (void)snprintf(out_reason, in_reason_size, "the display controller does not answer");

        return false;
    }

    if (!pacman_ai_is_available())
    {
        (void)snprintf(out_reason, in_reason_size, "the weight table cannot be evaluated on this build");

        return false;
    }

    /* The normal maze: the only one the game hands to the AI (FR-040), and the one the table was
     * evolved against. */
    game_session_start_on_normal_maze();

    if (!game_session_set_ai_enabled(true))
    {
        (void)snprintf(out_reason, in_reason_size, "the session refused to hand Pac-Man to the AI");

        return false;
    }

    uint32_t warmup_ms = 0U;
    uint32_t warmed = 0U;

    while (warmed < OTT_AI_FRAME_COST_WARMUP)
    {
        const uint32_t before = systick_bsp_get_tick();

        sw_timer_process();

        if (game_session_service())
        {
            warmup_ms += systick_bsp_get_tick() - before;
            ++warmed;
        }
    }

    const uint32_t start_tick = systick_bsp_get_tick();

    while (frames < OTT_AI_FRAME_COST_FRAMES)
    {
        const uint32_t before = systick_bsp_get_tick();

        sw_timer_process();

        if (game_session_service())
        {
            const uint32_t spent = systick_bsp_get_tick() - before;

            busy_ms += spent;

            if (spent > worst_ms)
            {
                /* The worst single frame, not only the average: a mean inside the budget with one
                 * frame far outside it is a stutter somebody sees, and an average hides it. */
                worst_ms = spent;
            }

            ++frames;
        }
    }

    const uint32_t elapsed_ms = systick_bsp_get_tick() - start_tick;
    const uint32_t achieved_fps = (frames * OTT_AI_FRAME_COST_MS_PER_SEC) / ((elapsed_ms > 0U) ? elapsed_ms : 1U);
    const uint32_t nominal_fps = OTT_AI_FRAME_COST_MS_PER_SEC / GAME_SESSION_FRAME_PERIOD_MS;
    const uint32_t required_fps = (nominal_fps * OTT_AI_FRAME_COST_MIN_RATE) / 100U;

    cli_print("  %lu warm-up frames cost %lu ms — the level's first full draw is in there", (unsigned long)warmed,
              (unsigned long)warmup_ms);
    cli_print("  %lu frames in %lu ms -> %lu fps (nominal %lu)", (unsigned long)frames, (unsigned long)elapsed_ms,
              (unsigned long)achieved_fps, (unsigned long)nominal_fps);
    cli_print("  %lu ms mean, %lu ms worst, of the %u ms budget", (unsigned long)(busy_ms / frames),
              (unsigned long)worst_ms, (unsigned)GAME_SESSION_FRAME_PERIOD_MS);

    if (achieved_fps < required_fps)
    {
        /* The figures go in the reason, not only in the log: the harness reports the reason and
         * swallows what the test printed, and a bare "too slow" is not something anybody can act
         * on. */
        (void)snprintf(out_reason, in_reason_size, "%lu fps against %lu required; %lu ms mean, %lu ms worst of %u",
                       (unsigned long)achieved_fps, (unsigned long)required_fps, (unsigned long)(busy_ms / frames),
                       (unsigned long)worst_ms, (unsigned)GAME_SESSION_FRAME_PERIOD_MS);

        return false;
    }

    /* Strictly greater. A frame measured *at* the budget fits in it, and the tick this is counted
     * in is 1 ms wide, so the budget itself is the last value that is not an overrun. Missing a
     * frame outright shows up in the rate above, which is the criterion that actually matters. */
    if (worst_ms > (uint32_t)GAME_SESSION_FRAME_PERIOD_MS)
    {
        (void)snprintf(out_reason, in_reason_size, "one frame took %lu ms of a %u ms budget, mean %lu, %lu fps",
                       (unsigned long)worst_ms, (unsigned)GAME_SESSION_FRAME_PERIOD_MS,
                       (unsigned long)(busy_ms / frames), (unsigned long)achieved_fps);

        return false;
    }

    return true;
}
