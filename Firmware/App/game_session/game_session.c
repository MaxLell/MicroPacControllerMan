#include "game_session.h"

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "game.h"
#include "game_view.h"
#include "msg.h"
#include "pacman_lookahead.h"
#include "render.h"
#include "sw_timer.h"

/* ==========================================================================
 * game_session - private
 * ========================================================================= */

static game_t g_game;
static game_view_t g_view;
static sw_timer_t g_frame_timer;
static bool g_is_frame_due;

/* The level whose maze the view was last given, so a new one is handed over exactly once.
 * `GAME_SESSION_NO_LEVEL` is not a level, so the first state always counts as a change. */
#define GAME_SESSION_NO_LEVEL (0U)
static uint8_t g_drawn_maze_level;

/* A timer is *registered* once and armed as often as you like: `sw_timer_create` asserts on
 * a second registration of the same instance, and #game_session_init is called again for
 * every run the player starts. */
static bool g_is_timer_created;

/* Whether the agent is playing, and where it last asked for something.
 *
 * The cell is remembered because a decision belongs to a *cell*, not to a frame: Pacman can only
 * turn on a cell boundary (§10.1), so a decision taken inside one would be thrown away. At level-1
 * speed a cell lasts about ten frames, which makes deciding per cell ten times cheaper than
 * deciding per frame and costs nothing at all in play. */
static game_session_player_e g_player;
static uint8_t g_ai_decided_column;
static uint8_t g_ai_decided_row;
static bool g_has_ai_decided;

static void prv_on_frame_due(void)
{
    g_is_frame_due = true;

    /* `sw_timer` is one-shot, so a callback re-arms its own timer to make it periodic.
     * Forgetting this line ran exactly one frame — and the first frame is the field
     * handover, which deliberately draws no actors, so the panel showed a maze and then
     * nothing at all, for ever. */
    sw_timer_start(&g_frame_timer, GAME_SESSION_FRAME_PERIOD_MS, prv_on_frame_due);
}

/* Whether Pacman has reached a cell the agent has not answered for yet. */
static bool prv_is_new_cell(const msg_game_state_t* const in_state)
{
    const bool is_new = !g_has_ai_decided || (in_state->pacman.column != g_ai_decided_column)
                        || (in_state->pacman.row != g_ai_decided_row);

    if (is_new)
    {
        g_has_ai_decided = true;
        g_ai_decided_column = in_state->pacman.column;
        g_ai_decided_row = in_state->pacman.row;
    }

    return is_new;
}

/* The search thinks for as long as the cell lasts, a slice a frame.
 *
 * **A decision belongs to a cell, and a cell is about ten frames long.** The search used to do all
 * of its thinking in the first of them and idle through the rest, which bought 1.63 junctions of
 * look-ahead where the same code reaches three given the time
 * ([M6 §15.6](../../../Docu/Design/M6-Pacman-AI.md)). It is rooted where it always was — the board
 * as Pacman entered the cell — and it takes effect where it always did, because `pacman_set_intent`
 * queues and he adopts the queue when his step falls due. What changed is only how deep the answer
 * is by the time he steps.
 *
 * The direction is handed over on **every** frame rather than only when it changes. It costs
 * nothing — the game stores an intent — and it is what makes a deeper answer arriving four frames
 * in replace a shallower one that was already on the queue. */
static void prv_service_lookahead(const msg_game_state_t* const in_state)
{
    const bool is_new_cell = prv_is_new_cell(in_state);

    if (is_new_cell)
    {
        pacman_lookahead_restart(&g_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_ANYTIME_TICK_BUDGET);
    }

    /* The first frame of a decision thinks less, because it has already spent milliseconds this frame
     * on the root copy and the food field — see #PACMAN_LOOKAHEAD_RESTART_SLICE_TICKS. */
    (void)pacman_lookahead_think(is_new_cell ? PACMAN_LOOKAHEAD_RESTART_SLICE_TICKS
                                             : PACMAN_LOOKAHEAD_FRAME_SLICE_TICKS);

    game_set_direction(&g_game, pacman_lookahead_get_direction());
}

/* Let the agent choose, if it is playing and Pacman has reached a cell it has not answered for.
 *
 * Asking `game` for the direction rather than going through #game_session_set_direction on
 * purpose: that door is shut while the AI plays (FR-031), and the AI must not be shut out of it. */
static void prv_service_ai(const msg_game_state_t* const in_state)
{
    if (g_player == GAME_SESSION_PLAYER_HUMAN)
    {
        return;
    }

    prv_service_lookahead(in_state);
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
    g_drawn_maze_level = GAME_SESSION_NO_LEVEL;

    if (!g_is_timer_created)
    {
        g_is_timer_created = true;

        sw_timer_create(&g_frame_timer);
    }

    sw_timer_start(&g_frame_timer, GAME_SESSION_FRAME_PERIOD_MS, prv_on_frame_due);
}

/* Every run begins under player control (FR-033), and neither the AI's last decision nor the HUD's
 * last word about it carries over. */
static void prv_start_under_player_control(void)
{
    g_player = GAME_SESSION_PLAYER_HUMAN;
    g_has_ai_decided = false;

    game_view_set_player(&g_view, GAME_SESSION_PLAYER_HUMAN);
}

void game_session_start(uint32_t in_maze_seed)
{
    game_start(&g_game, in_maze_seed);

    /* The run's mazes are new, so nothing the view holds applies to them. */
    g_drawn_maze_level = GAME_SESSION_NO_LEVEL;

    prv_start_under_player_control();
}

void game_session_start_on_map(const playfield_map_t* in_map)
{
    game_start_on_map(&g_game, in_map);

    g_drawn_maze_level = GAME_SESSION_NO_LEVEL;

    prv_start_under_player_control();
}

void game_session_start_on_normal_maze(void)
{
    game_start_on_normal_maze(&g_game);

    g_drawn_maze_level = GAME_SESSION_NO_LEVEL;

    prv_start_under_player_control();
}

void game_session_set_direction(direction_e in_direction)
{
    /* FR-031: while a machine plays, the stick is dead. One place, so every caller is covered. */
    if (g_player != GAME_SESSION_PLAYER_HUMAN)
    {
        return;
    }

    game_set_direction(&g_game, in_direction);
}

bool game_session_set_player(game_session_player_e in_player)
{
    /* **Nothing can refuse any more, and the return value is kept anyway.** The trained network
     * could be *unavailable* — its weights were generated and a generator can be wrong — which is
     * what this reported. The search has nothing to be wrong about: it reads the run it is given.
     * The `bool` stays because the caller's own logic is built on being told, and a signature that
     * can only say `true` is cheaper to keep than a shell that has to be re-argued. */
    g_player = in_player;

    /* So that taking over acts at the next boundary rather than waiting for Pacman to leave the
     * cell he was already in when the button was pressed. */
    g_has_ai_decided = false;

    game_view_set_player(&g_view, in_player);

    return true;
}

bool game_session_set_ai_enabled(bool in_is_enabled)
{
    return game_session_set_player(in_is_enabled ? GAME_SESSION_PLAYER_LOOKAHEAD : GAME_SESSION_PLAYER_HUMAN);
}

game_session_player_e game_session_get_player(void)
{
    return g_player;
}

void game_session_set_infinite(bool in_is_infinite)
{
    /* Straight through to the view: the session has no opinion about the loop, it only owns the
     * frame the HUD is drawn in. The shell owns whether the loop is on, because the shell owns what
     * happens when a run ends. */
    game_view_set_infinite(&g_view, in_is_infinite);
}

bool game_session_is_ai_enabled(void)
{
    return g_player != GAME_SESSION_PLAYER_HUMAN;
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

    prv_service_ai(&state);

    /* A level's maze reaches the view here, and only when it changes. It cannot ride along
     * inside the state message: the state is 246 bytes of a 256-byte payload and a maze does
     * not fit in what is left. It does not need to — a maze changes once a level, where the
     * state changes every step. */
    if (g_drawn_maze_level != state.level)
    {
        g_drawn_maze_level = state.level;

        game_view_set_maze(&g_view, game_get_maze(&g_game));
    }

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

void game_session_get_state_message(msg_game_state_t* out_state)
{
    game_get_state_message(&g_game, out_state);
}

#if defined(TEST)
/* Forgets that the frame timer was ever registered, so a test may reset `sw_timer` under
 * this module and start clean. Not built into the firmware, where the two are brought up
 * once and in order. */
void game_session_test_reset(void)
{
    g_is_timer_created = false;
}
#endif /* defined(TEST) */
