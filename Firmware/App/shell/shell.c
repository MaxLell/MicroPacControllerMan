#include "shell.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "framebuffer.h"
#include "game.h"
#include "game_session.h"
#include "high_score.h"
#include "msg.h"
#include "playfield.h"
#include "render.h"
#include "rng_bsp.h"
#include "sprite_set.h"
#include "sw_timer.h"

/* ==========================================================================
 * shell - private
 * ========================================================================= */

#define GLYPH_SIZE          (8)
#define ACTOR_SIZE          (16)

/* The five actors of the title row, spaced so they do not touch. */
#define TITLE_ACTOR_COUNT   (5)
#define TITLE_ACTOR_PITCH   (24)

/* The game's own name, hyphen and all. It read `PACMAN` for a long time, because the font decoded
 * out of the tile ROM is the digits, the letters and a space — so this is the one string on the
 * panel that needed a glyph drawn rather than decoded (`SPRITE_SET_GLYPH_HYPHEN`). A hyphen in an
 * 8 x 8 monospace font is not the invention the project avoids elsewhere; a logo bitmap would be. */
#define TITLE_TEXT          "PAC-MAN"

#define SCORE_DIGITS        (7U)
#define PLACE_DIGITS        (1U)

/* Where the two word screens put their rows, in panel pixels. Written as named constants
 * rather than arithmetic in the drawing code, because a layout is read far more often than
 * it is computed. */
#define LOADING_TITLE_Y     (136)
#define LOADING_ACTORS_Y    (168)

/* The menu carries the scores and the choice of game and nothing else — no title and no cast.
 * Both were on it because it used to be the only screen a player waited on; the loading screen
 * shows them 3 s earlier, and a screen that has to be *read* is better off without them. The rows
 * are placed so that the block from the heading to the last option is centred on the panel. */
#define MENU_HEADING_Y      (56)
#define MENU_FIRST_SCORE_Y  (80)
#define MENU_SCORE_PITCH    (16)
#define MENU_FIRST_OPTION_Y (176)
#define MENU_OPTION_PITCH   (40)

/* An option's note sits one row under it, which is what makes it read as a note about that
 * option rather than as a third choice. */
#define MENU_NOTE_OFFSET_Y  (16)

/* Pacman marks the selection, one sprite's width plus a gap to the left of the label, and raised
 * so his 16 px sit centred on an 8 px row of text. */
#define MENU_CURSOR_GAP     (8)
#define MENU_CURSOR_RISE    ((ACTOR_SIZE - GLYPH_SIZE) / 2)

#define SCORE_HEADING_Y     (120)
#define SCORE_VALUE_Y       (152)
#define SCORE_NOTE_Y        (184)

/* The palette each actor of the title row is drawn in, in the order they stand. */
static const sprite_set_palette_e g_title_palettes[TITLE_ACTOR_COUNT] = {
    SPRITE_SET_PALETTE_PACMAN, SPRITE_SET_PALETTE_BLINKY, SPRITE_SET_PALETTE_PINKY,
    SPRITE_SET_PALETTE_INKY,   SPRITE_SET_PALETTE_CLYDE,
};

/* What the menu offers, in the order it lists them (FR-040), and each entry is also the index of
 * that game's high-score table (FR-041).
 *
 * The words are upper case because that is the font, and a note goes *under* a label rather than in
 * brackets after it — the font has one piece of punctuation, the hyphen drawn for the title.
 *
 * The labels are not all the same length any more, and that is why the cursor is placed from the
 * label rather than at a column of its own: `PAC-MAN AI` is one character shorter than the two
 * mazes, so a fixed column would leave it hanging. */
static const struct
{
    const char* label;
    const char* note; /*!< A second line under the label, or `NULL` */
    const char* name; /*!< For a caller reporting on the console    */
} g_menu_options[SHELL_MODE_COUNT] = {
    {"NORMAL MAZE", "AI ON DEMAND", "normal maze"},
    /* No note: the AI game's second line is #g_selected_ai's name, drawn with the selection because
     * it changes with it. `PLAYS ITSELF` was there and is what the label already says. */
    {"PAC-MAN AI", NULL, "pac-man ai"},
    {"RANDOM MAZE", NULL, "random maze"},
};

/* The games and their tables are the same set counted twice, so let the build say so rather than a
 * comment. */
_Static_assert((uint8_t)SHELL_MODE_COUNT == HIGH_SCORE_TABLE_COUNT, "one high-score table per game");

static shell_screen_e g_screen;
static sw_timer_t g_timer;
static bool g_is_drawn;
static bool g_has_logo_appeared;
static uint32_t g_last_score;
static bool g_was_last_score_a_high_score;

/* Which game the menu is offering, and — because the selection can only be moved on the menu —
 * which game the run in progress is. Also the index of the high-score table that game keeps. */
static shell_mode_e g_selected_mode;

/*! \brief Which machine the agent's own game hands Pac-Man to (FR-042).
 *
 * A *menu* choice and not an in-game one: the run is the agent's from the first frame, so there is
 * no moment during it at which a player is deciding anything, and the button that would carry the
 * choice already means the endless mode (FR-043). It is also the honest place for it — which agent
 * is playing is a property of the run, and picking it before the run starts is what makes two runs
 * comparable. */
static game_session_player_e g_selected_ai;

/* Whether a run that ends should be followed by another one (FR-043). Only ever set in the Pac-Man
 * AI game; cleared when a run is started from the menu, so a game begun by hand is one game. */
static bool g_is_infinite;

/* Runs the loop has played, the one in progress included. Reported rather than kept for its own
 * sake: an endless run of runs with no count is a board that looks stuck rather than busy. */
static uint32_t g_run_count;

/* Whether what is on the panel matches #g_selected_mode. Moving the selection redraws *the cursor
 * and the three scores* — the option words have not changed — rather than the screen: a menu that
 * blanked and rebuilt itself on every push of the stick would take a third of a second to answer
 * one. The scores are in it because each game keeps its own table (FR-041), so moving the selection
 * changes which three numbers are true. */
static bool g_is_selection_drawn;

/* Whether the AI played at any point in the run that is in progress (FR-034).
 *
 * A latch, not a state: the requirement is "was on at some point", so handing control back does not
 * clear it and only a new run does. `game_session` owns whether the AI is playing *now* — this is
 * the run's memory of it, and the run belongs to the shell. */
static bool g_has_ai_played;

static void prv_on_timeout(void)
{
    /* Nothing to do: #shell_service watches sw_timer_is_active(). */
}

/* Send whatever has accumulated, and start a fresh list.
 *
 * A display list holds eight items and a screen of words holds far more, so the drawing
 * below fills and flushes as it goes. Splitting is safe here in a way it is not for a game
 * frame: nothing on these screens moves, so a half-drawn one is a screen mid-draw rather
 * than a lie about where Pacman is. */
static void prv_flush(msg_display_list_t* inout_list)
{
    if (inout_list->count > 0U)
    {
        render_draw(inout_list);
        inout_list->count = 0U;
    }
}

static void prv_add(msg_display_list_t* inout_list, sprite_set_id_e in_sprite, sprite_set_palette_e in_palette,
                    int16_t in_x, int16_t in_y)
{
    msg_display_item_t* item;

    if (inout_list->count >= MSG_DISPLAY_ITEM_MAX)
    {
        prv_flush(inout_list);
    }

    item = &inout_list->items[inout_list->count];

    /* Background, not actor: nothing here moves, so there is nothing to save the pixels
     * under. Asking Render to save-under a screen of text would spend its whole budget on
     * restoring pixels that are about to be overwritten anyway. */
    item->kind = (uint8_t)DISPLAY_ITEM_BACKGROUND;
    item->drawing.sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
    item->x = in_x;
    item->y = in_y;

    ++inout_list->count;
}

/* The one thing on these screens that moves, and therefore the one thing that goes in as an actor:
 * Render saves the pixels under an actor and puts them back before it draws the next list, so
 * moving the cursor costs the two rectangles it was and is — not a redraw of the menu. */
static void prv_add_cursor(msg_display_list_t* inout_list, int16_t in_x, int16_t in_y)
{
    msg_display_item_t* item;

    if (inout_list->count >= MSG_DISPLAY_ITEM_MAX)
    {
        prv_flush(inout_list);
    }

    item = &inout_list->items[inout_list->count];

    item->kind = (uint8_t)DISPLAY_ITEM_ACTOR;
    item->drawing.sprite = (uint8_t)SPRITE_SET_PACMAN_WIDE_EAST;
    item->palette = (uint8_t)SPRITE_SET_PALETTE_PACMAN;
    item->x = in_x;
    item->y = in_y;

    ++inout_list->count;
}

static int16_t prv_get_centred_x(uint8_t in_character_count)
{
    return (int16_t)((FRAMEBUFFER_WIDTH - (in_character_count * GLYPH_SIZE)) / 2);
}

static void prv_draw_text(msg_display_list_t* inout_list, int16_t in_x, int16_t in_y, const char* const in_text)
{
    int16_t x = in_x;

    for (size_t index = 0U; in_text[index] != '\0'; ++index)
    {
        prv_add(inout_list, sprite_set_get_glyph(in_text[index]), SPRITE_SET_PALETTE_TEXT, x, in_y);

        x = (int16_t)(x + GLYPH_SIZE);
    }
}

static void prv_draw_centred_text(msg_display_list_t* inout_list, int16_t in_y, const char* const in_text)
{
    prv_draw_text(inout_list, prv_get_centred_x((uint8_t)strlen(in_text)), in_y, in_text);
}

/* A number, right-aligned in a fixed field, leading zeros drawn as zeros.
 *
 * Zeros rather than blanks, unlike the HUD: a score screen shows one number and a row of
 * `0000000` reads as a score, where the HUD's blanks keep a changing number from looking
 * like it is jumping about. */
static void prv_draw_number(msg_display_list_t* inout_list, int16_t in_x, int16_t in_y, uint32_t in_value,
                            uint8_t in_digits)
{
    uint32_t remaining = in_value;

    for (uint8_t digit = 0U; digit < in_digits; ++digit)
    {
        const int16_t x = (int16_t)(in_x + ((in_digits - 1U - digit) * GLYPH_SIZE));

        prv_add(inout_list, sprite_set_get_glyph((char)('0' + (remaining % 10U))), SPRITE_SET_PALETTE_TEXT, x, in_y);

        remaining /= 10U;
    }
}

/* Pacman and the four ghosts in a row — the arcade's own cast, and the closest thing to a
 * logo that exists in the tile ROM. */
static void prv_draw_title_actors(msg_display_list_t* inout_list, int16_t in_y)
{
    static const sprite_set_id_e k_sprites[TITLE_ACTOR_COUNT] = {
        SPRITE_SET_PACMAN_WIDE_EAST, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_GHOST_EAST_A,
        SPRITE_SET_GHOST_EAST_A,     SPRITE_SET_GHOST_EAST_A,
    };
    const int16_t width = (TITLE_ACTOR_COUNT * TITLE_ACTOR_PITCH) - (TITLE_ACTOR_PITCH - ACTOR_SIZE);
    const int16_t first_x = (int16_t)((FRAMEBUFFER_WIDTH - width) / 2);

    for (uint8_t index = 0U; index < TITLE_ACTOR_COUNT; ++index)
    {
        prv_add(inout_list, k_sprites[index], g_title_palettes[index], (int16_t)(first_x + (index * TITLE_ACTOR_PITCH)),
                in_y);
    }
}

static void prv_draw_loading(void)
{
    msg_display_list_t list = {0};

    prv_draw_centred_text(&list, LOADING_TITLE_Y, TITLE_TEXT);
    prv_draw_title_actors(&list, LOADING_ACTORS_Y);
    prv_flush(&list);
}

static int16_t prv_get_option_y(shell_mode_e in_mode)
{
    return (int16_t)(MENU_FIRST_OPTION_Y + ((int16_t)in_mode * MENU_OPTION_PITCH));
}

/* Where the cursor stands for an option: to the left of that option's label, which is centred, so
 * the cursor follows the label rather than being placed at a column of its own. That matters now
 * that the labels are not all the same length. */
static int16_t prv_get_cursor_x(shell_mode_e in_mode)
{
    const int16_t label_x = prv_get_centred_x((uint8_t)strlen(g_menu_options[in_mode].label));

    return (int16_t)(label_x - ACTOR_SIZE - MENU_CURSOR_GAP);
}

/* The three scores of the selected game (FR-041). */
static void prv_draw_scores(msg_display_list_t* inout_list)
{
    for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        /* One place digit, a space, then the score: eleven columns, centred as a block so
         * the three rows line up under each other whatever they hold. */
        const int16_t x = prv_get_centred_x((uint8_t)(PLACE_DIGITS + 2U + SCORE_DIGITS));
        const int16_t y = (int16_t)(MENU_FIRST_SCORE_Y + (place * MENU_SCORE_PITCH));

        prv_draw_number(inout_list, x, y, (uint32_t)(place + 1U), PLACE_DIGITS);
        prv_draw_number(inout_list, (int16_t)(x + ((PLACE_DIGITS + 2U) * GLYPH_SIZE)), y,
                        high_score_get((uint8_t)g_selected_mode, place), SCORE_DIGITS);
    }
}

/* What a change of selection puts on the panel: the three scores of the game now selected, and the
 * cursor beside it.
 *
 * The cursor goes **last and in a list of its own**, because Render restores the pixels under the
 * actors of the *previous* list — so a background list drawn after it would wipe it. The scores
 * before it are the reason this is not simply "move the cursor": each game keeps its own table, so
 * the three numbers above the list are part of the selection. */
/*! \brief The AI names as the menu shows them, indexed by `game_session_player_e`.
 *
 * Both padded to the same width on purpose: they are redrawn in place when the choice moves, and a
 * shorter name would leave the tail of a longer one on the panel.
 *
 * `AGENT` in front of the name rather than arrows around it, which is what this wanted to be: the
 * font is the 1980 ROM's, and that is letters, digits, a space and the one hyphen that had to be
 * drawn by hand — there is no `<` or `>` to have. The word does the arrows' other job anyway, which
 * is to say that this line is a property of the game above it and not a fourth game. */
static const char* const g_menu_ai_labels[] = {NULL, "AGENT NEAT  ", "AGENT SEARCH"};

static void prv_draw_selection(void)
{
    msg_display_list_t list = {0};

    prv_draw_scores(&list);
    prv_flush(&list);

    /* Which agent the AI game will use, under its label. Here rather than in #prv_draw_menu because
     * it changes without the menu being redrawn — left and right move it — and it has to be
     * redrawn when it does.
     *
     * In a flush of its own, because a display list has a fixed capacity and the three score rows
     * very nearly fill one. Appending to theirs overran it, which the list's own precondition
     * caught. */
    prv_draw_centred_text(&list, (int16_t)(prv_get_option_y(SHELL_MODE_AI) + MENU_NOTE_OFFSET_Y),
                          g_menu_ai_labels[g_selected_ai]);
    prv_flush(&list);

    prv_add_cursor(&list, prv_get_cursor_x(g_selected_mode),
                   (int16_t)(prv_get_option_y(g_selected_mode) - MENU_CURSOR_RISE));
    prv_flush(&list);

    g_is_selection_drawn = true;
}

static void prv_draw_menu(void)
{
    msg_display_list_t list = {0};

    render_init();

    prv_draw_centred_text(&list, MENU_HEADING_Y, "HIGH SCORES");

    for (uint8_t option = 0U; option < (uint8_t)SHELL_MODE_COUNT; ++option)
    {
        const int16_t y = prv_get_option_y((shell_mode_e)option);

        prv_draw_centred_text(&list, y, g_menu_options[option].label);

        if (g_menu_options[option].note != NULL)
        {
            prv_draw_centred_text(&list, (int16_t)(y + MENU_NOTE_OFFSET_Y), g_menu_options[option].note);
        }
    }

    prv_flush(&list);
    prv_draw_selection();
}

static void prv_draw_score(void)
{
    msg_display_list_t list = {0};
    const bool has_won = (game_session_get_state() == GAME_STATE_WON);

    render_init();

    prv_draw_centred_text(&list, SCORE_HEADING_Y, has_won ? "YOU WIN" : "GAME OVER");
    prv_draw_number(&list, prv_get_centred_x(SCORE_DIGITS), SCORE_VALUE_Y, g_last_score, SCORE_DIGITS);

    if (g_was_last_score_a_high_score)
    {
        prv_draw_centred_text(&list, SCORE_NOTE_Y, "NEW HIGH SCORE");
    }

    prv_flush(&list);
}

static void prv_enter(shell_screen_e in_screen, uint32_t in_duration_ms)
{
    g_screen = in_screen;
    g_is_drawn = false;

    sw_timer_stop(&g_timer);

    if (in_duration_ms > 0U)
    {
        sw_timer_start(&g_timer, in_duration_ms, prv_on_timeout);
    }
}

/* A run has ended: offer it to the table before the score screen shows it, so the screen
 * can say whether it got in.
 *
 * Here rather than in `game`, and once per run rather than as the score climbs: storing
 * erases a flash page and stalls the CPU for milliseconds, which inside a frame would be a
 * visible stutter ([10 §10.11](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)). */
static void prv_finish_run(void)
{
    /* FR-034 applies to the games a *person* plays: a run the agent touched is not that person's
     * run, so it is not offered at all — however high it scored, and whether or not control was
     * handed back before the end. The sticky flag rather than the live one, because handing back
     * just before the last life would otherwise launder the score.
     *
     * The Pac-Man AI game is the exception, and not one: its table is the agent's table (FR-041), so
     * an AI run entering it launders nothing. Refusing every score there would leave one of the
     * three tables permanently empty. */
    const bool is_the_agents_own_game = (g_selected_mode == SHELL_MODE_AI);
    const bool is_locked_out = g_has_ai_played && !is_the_agents_own_game;

    g_last_score = game_session_get_score();
    g_was_last_score_a_high_score = is_locked_out ? false : high_score_offer((uint8_t)g_selected_mode, g_last_score);

    prv_enter(SHELL_SCREEN_SCORE, SHELL_SCORE_MS);
}

/* Begin a run of the selected game.
 *
 * The one place a run starts, because there are two ways in now — a player pressing start, and the
 * endless loop starting the next one (FR-043) — and they must set a run up identically. A second
 * copy is how the loop would end up playing a subtly different game from the first run.
 *
 * \return `false` when the game could not be started, which today means only one thing: the
 *         Pac-Man AI game was asked for and the weight table cannot be evaluated. */
static bool prv_start_run(void)
{
    /* `game_session_init` re-initialises Render, which is what clears the previous screen off the
     * panel and gives the field handover a clean buffer to draw into. */
    game_session_init();

    if (g_selected_mode == SHELL_MODE_RANDOM_MAZE)
    {
        /* The run's mazes are seeded from the random source (FR-029/FR-045), which on the board is
         * the MCU's own RNG peripheral. It used to be the tick at the moment start was pressed —
         * entropy read off a clock, which worked because a player cannot press a key on a chosen
         * millisecond, but which is a weak source next to a peripheral built to be one. The seed is
         * still a *seed*: `maze_gen` remains reproducible from it, which is what lets a maze be
         * replayed. */
        game_session_start(rng_bsp_get_u32());
    }
    else
    {
        /* Both of the other two games are the arcade's own maze: it is the one the agent was
         * trained on, so it is the one it is offered in and the only one it can play alone. */
        game_session_start_on_normal_maze();
    }

    g_has_ai_played = false;

    if (g_selected_mode == SHELL_MODE_AI)
    {
        /* Whichever agent the menu is showing has Pac-Man from the first frame and keeps him
         * (FR-042). Refused only when the generated table cannot be evaluated — and then the run
         * does not start at all rather than falling back to the player, because a game that says it
         * plays itself must either do that or say it cannot. The search cannot refuse, so choosing
         * it is also the way to play this game on a firmware whose weights are broken. */
        if (!game_session_set_player(g_selected_ai))
        {
            return false;
        }

        g_has_ai_played = true;
    }

    prv_enter(SHELL_SCREEN_GAME, 0U);

    return true;
}

static bool prv_service_loading(void)
{
    /* Dark first, then the title (NFR-005). The delay is the whole of the first phase, so
     * "has the logo appeared" is the only state it needs. */
    if (!g_has_logo_appeared && (sw_timer_is_active(&g_timer)))
    {
        return false;
    }

    if (!g_has_logo_appeared)
    {
        g_has_logo_appeared = true;

        /* No blanking here: #shell_init already left the panel dark, and that darkness is
         * the first 200 ms of NFR-005. */
        prv_draw_loading();

        sw_timer_start(&g_timer, SHELL_LOADING_MS - SHELL_LOGO_DELAY_MS, prv_on_timeout);

        return true;
    }

    if (!sw_timer_is_active(&g_timer))
    {
        prv_enter(SHELL_SCREEN_MENU, 0U);
    }

    return false;
}

/* ==========================================================================
 * shell - public
 * ========================================================================= */

void shell_init(void)
{
    g_has_logo_appeared = false;
    g_last_score = 0U;
    g_was_last_score_a_high_score = false;

    /* The normal maze, because it is the one a first-time player should meet: it is the arcade's
     * layout, and it is the only one that offers the AI. Only #shell_init sets it — coming back
     * from a run leaves the menu on whatever was last played. */
    g_selected_mode = SHELL_MODE_NORMAL_MAZE;
    g_selected_ai = GAME_SESSION_PLAYER_NETWORK;
    g_is_selection_drawn = false;
    g_is_infinite = false;
    g_run_count = 0U;

    render_init();

    sw_timer_create(&g_timer);

    prv_enter(SHELL_SCREEN_LOADING, SHELL_LOGO_DELAY_MS);
}

bool shell_service(void)
{
    bool did_draw = false;

    switch (g_screen)
    {
        case SHELL_SCREEN_LOADING: did_draw = prv_service_loading(); break;

        case SHELL_SCREEN_MENU:
            if (!g_is_drawn)
            {
                g_is_drawn = true;
                did_draw = true;

                prv_draw_menu();
            }
            else if (!g_is_selection_drawn)
            {
                did_draw = true;

                prv_draw_selection();
            }
            else
            {
                /* Up, and waiting for a player. */
            }
            break;

        case SHELL_SCREEN_GAME:
            did_draw = game_session_service();

            if (game_session_get_state() != GAME_STATE_RUNNING)
            {
                prv_finish_run();
            }
            break;

        case SHELL_SCREEN_SCORE:
            if (!g_is_drawn)
            {
                g_is_drawn = true;
                did_draw = true;

                prv_draw_score();
            }
            else if (!sw_timer_is_active(&g_timer))
            {
                /* The loop of FR-043: the next run instead of the menu. The score screen still has
                 * its two seconds first — the run's result is worth seeing, and at three lives a run
                 * the pause is a rounding error against the game it follows.
                 *
                 * `prv_start_run` cannot fail here: the loop only ever runs in the Pac-Man AI game,
                 * and that game would not have started if the table were unusable. Checked anyway,
                 * because "cannot happen" and "is not checked" is how a loop turns into a board
                 * sitting on a score screen for ever. */
                if (g_is_infinite && (g_selected_mode == SHELL_MODE_AI) && prv_start_run())
                {
                    ++g_run_count;
                }
                else
                {
                    prv_enter(SHELL_SCREEN_MENU, 0U);
                }
            }
            else
            {
                /* Still being read. */
            }
            break;

        default: ASSERT(false); break;
    }

    return did_draw;
}

void shell_press_start(void)
{
    if (g_screen == SHELL_SCREEN_MENU)
    {
        /* A game begun by hand is one game: the loop is something the player switches on during a
         * run, so starting from the menu leaves it off (FR-043). */
        g_is_infinite = false;
        g_run_count = 1U;

        if (!prv_start_run())
        {
            /* Back on the menu with nothing changed. The caller reports it — the shell has no
             * console — and the menu is still up, so the player can choose another game. */
            g_run_count = 0U;
        }
    }
    else if (g_screen == SHELL_SCREEN_SCORE)
    {
        prv_enter(SHELL_SCREEN_MENU, 0U);
    }
    else
    {
        /* Loading cannot be skipped and a run in progress cannot be restarted. */
    }
}

void shell_set_direction(direction_e in_direction)
{
    if (g_screen == SHELL_SCREEN_GAME)
    {
        game_session_set_direction(in_direction);
    }
}

void shell_move_selection(direction_e in_direction)
{
    if (g_screen != SHELL_SCREEN_MENU)
    {
        return;
    }

    /* Sideways picks the agent, and only where there is an agent to pick. Up and down move between
     * games, so left and right are the axis that was free — and putting the choice on the menu
     * keeps the in-game button meaning the one thing it already meant (FR-043). */
    if ((in_direction == DIRECTION_WEST) || (in_direction == DIRECTION_EAST))
    {
        if (g_selected_mode == SHELL_MODE_AI)
        {
            g_selected_ai = (g_selected_ai == GAME_SESSION_PLAYER_NETWORK) ? GAME_SESSION_PLAYER_LOOKAHEAD
                                                                           : GAME_SESSION_PLAYER_NETWORK;

            g_is_selection_drawn = false;
        }

        return;
    }

    shell_mode_e moved = g_selected_mode;

    /* Stops at the ends rather than wrapping: a player pushing up at the top of the list expects
     * nothing to happen, and with three entries wrapping would make the middle one reachable two
     * ways and the ends confusing. */
    if ((in_direction == DIRECTION_NORTH) && (g_selected_mode > 0))
    {
        moved = (shell_mode_e)(g_selected_mode - 1);
    }
    else if ((in_direction == DIRECTION_SOUTH) && (g_selected_mode < (SHELL_MODE_COUNT - 1)))
    {
        moved = (shell_mode_e)(g_selected_mode + 1);
    }
    else
    {
        /* Already at the end of the list. */
    }

    if (moved != g_selected_mode)
    {
        g_selected_mode = moved;

        /* Redrawn by #shell_service and not here, because a caller that blits the frame buffer into
         * a window does it when the service call says something reached the panel. */
        g_is_selection_drawn = false;
    }
}

shell_mode_e shell_get_selected_mode(void)
{
    return g_selected_mode;
}

const char* shell_get_mode_name(shell_mode_e in_mode)
{
    ASSERT(in_mode < SHELL_MODE_COUNT);

    return g_menu_options[in_mode].name;
}

void shell_press_user_button(void)
{
    /* The button means one thing per screen and per game, and the table in the header is this
     * function. It is a chain of "did that mean anything" rather than a switch, because each of the
     * two toggles already knows the whole of when it applies — a switch here would be a second copy
     * of those conditions, and the copy is what drifts. */
    if (shell_toggle_infinite())
    {
        return;
    }

    if (shell_toggle_ai())
    {
        return;
    }

    shell_press_start();
}

bool shell_toggle_ai(void)
{
    /* Only during a run: on the menu and the score screen the same button means start (FR-003), and
     * a toggle with no run to toggle would have to invent one. */
    if (g_screen != SHELL_SCREEN_GAME)
    {
        return false;
    }

    /* And only in the normal maze (FR-040/FR-042). Not the random maze, because the agent was
     * evolved against the arcade's layout and offering it on a maze nobody has ever played would be
     * offering something else; and not the Pac-Man AI game, where it plays start to finish and there
     * is nobody to hand back to. Enforced here rather than at each caller, for the same reason the
     * joystick lock-out lives in `game_session_set_direction`: one door, so it holds however many
     * devices are wired to it. */
    if (g_selected_mode != SHELL_MODE_NORMAL_MAZE)
    {
        return false;
    }

    /* The trained network only, and a plain toggle. The look-ahead search is **not** offered here:
     * the owner asked for it in the agent's own game, where the choice is made on the menu before
     * the run starts, rather than as a third state of a button in a game a person is playing. A
     * cycle was built here first and taken out again — the button had come to mean "step through
     * whatever machines exist", which is not what a player reaching for it wants. */
    if (!game_session_set_ai_enabled(!game_session_is_ai_enabled()))
    {
        return false;
    }

    /* Latched here rather than where control goes back to the player, so that the run remembers
     * what it cannot be told again. */
    g_has_ai_played = g_has_ai_played || game_session_is_ai_enabled();

    return true;
}

uint8_t shell_get_selected_ai(void)
{
    return (uint8_t)g_selected_ai;
}

bool shell_is_ai_playing(void)
{
    return (g_screen == SHELL_SCREEN_GAME) && game_session_is_ai_enabled();
}

bool shell_has_ai_played(void)
{
    return g_has_ai_played;
}

bool shell_toggle_infinite(void)
{
    /* Only during a Pac-Man AI run (FR-043). Not on the menu, where the button means start and where
     * arming a loop would be arming something the player cannot see the effect of; and not in a game
     * a person is playing, where a restart would replay a run they had not asked to replay. */
    if ((g_screen != SHELL_SCREEN_GAME) || (g_selected_mode != SHELL_MODE_AI))
    {
        return false;
    }

    g_is_infinite = !g_is_infinite;

    game_session_set_infinite(g_is_infinite);

    return true;
}

bool shell_is_infinite(void)
{
    return g_is_infinite;
}

uint32_t shell_get_run_count(void)
{
    return g_run_count;
}

shell_screen_e shell_get_screen(void)
{
    return g_screen;
}
