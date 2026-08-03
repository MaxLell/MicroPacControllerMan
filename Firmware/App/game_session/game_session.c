#include "game_session.h"

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "game.h"
#include "game_view.h"
#include "msg.h"
#include "render.h"
#include "sw_timer.h"

/* ==========================================================================
 * game_session - private
 * ========================================================================= */

static game_t g_game;
static game_view_t g_view;
static sw_timer_t g_frame_timer;
static bool g_is_frame_due;

static void prv_on_frame_due(void)
{
    g_is_frame_due = true;

    /* `sw_timer` is one-shot, so a callback re-arms its own timer to make it periodic.
     * Forgetting this line ran exactly one frame — and the first frame is the field
     * handover, which deliberately draws no actors, so the panel showed a maze and then
     * nothing at all, for ever. */
    sw_timer_start(&g_frame_timer, GAME_SESSION_FRAME_PERIOD_MS, prv_on_frame_due);
}

/* ==========================================================================
 * game_session - public
 * ========================================================================= */

void game_session_init(void)
{
    render_init();

    game_init(&g_game);
    game_view_init(&g_view);

    g_is_frame_due = false;

    sw_timer_create(&g_frame_timer);
    sw_timer_start(&g_frame_timer, GAME_SESSION_FRAME_PERIOD_MS, prv_on_frame_due);
}

void game_session_start(void)
{
    game_start(&g_game);
}

void game_session_set_direction(direction_e in_direction)
{
    game_set_direction(&g_game, in_direction);
}

bool game_session_service(void)
{
    msg_game_state_t state;
    msg_display_list_t list;

    if (!g_is_frame_due)
    {
        return false;
    }

    g_is_frame_due = false;

    game_tick(&g_game, GAME_SESSION_FRAME_PERIOD_MS);

    game_get_state_message(&g_game, &state);
    game_view_set_state(&g_view, &state);

    /* A level change hands the whole field over across several lists; an ordinary frame is
     * one. Both are drained here so a frame is never left half-drawn. */
    do
    {
        if (game_view_get_display_list(&g_view, &list))
        {
            render_draw(&list);
        }
    } while (game_view_is_field_pending(&g_view));

    display_service();

    return true;
}

game_state_e game_session_get_state(void)
{
    return game_get_state(&g_game);
}

uint32_t game_session_get_score(void)
{
    return game_get_score(&g_game);
}

uint8_t game_session_get_lives(void)
{
    return game_get_lives(&g_game);
}

uint8_t game_session_get_level(void)
{
    return game_get_level(&g_game);
}
