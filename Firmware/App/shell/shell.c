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
#include "render.h"
#include "sprite_set.h"
#include "sw_timer.h"

/* ==========================================================================
 * shell - private
 * ========================================================================= */

#define GLYPH_SIZE         (8)
#define ACTOR_SIZE         (16)

/* The five actors of the title row, spaced so they do not touch. */
#define TITLE_ACTOR_COUNT  (5)
#define TITLE_ACTOR_PITCH  (24)

/* No hyphen: the font decoded out of the tile ROM is the digits, the letters and a space,
 * and the arcade's "PAC-MAN" would need a glyph this project does not have. Drawing one
 * would be the invention being avoided everywhere else. */
#define TITLE_TEXT         "PACMAN"

#define SCORE_DIGITS       (7U)
#define PLACE_DIGITS       (1U)

/* Where the two word screens put their rows, in panel pixels. Written as named constants
 * rather than arithmetic in the drawing code, because a layout is read far more often than
 * it is computed. */
#define LOADING_TITLE_Y    (136)
#define LOADING_ACTORS_Y   (168)

#define MENU_TITLE_Y       (48)
#define MENU_ACTORS_Y      (80)
#define MENU_HEADING_Y     (136)
#define MENU_FIRST_SCORE_Y (160)
#define MENU_SCORE_PITCH   (16)
#define MENU_PROMPT_Y      (248)

#define SCORE_HEADING_Y    (120)
#define SCORE_VALUE_Y      (152)
#define SCORE_NOTE_Y       (184)

/* The palette each actor of the title row is drawn in, in the order they stand. */
static const sprite_set_palette_e g_title_palettes[TITLE_ACTOR_COUNT] = {
    SPRITE_SET_PALETTE_PACMAN, SPRITE_SET_PALETTE_BLINKY, SPRITE_SET_PALETTE_PINKY,
    SPRITE_SET_PALETTE_INKY,   SPRITE_SET_PALETTE_CLYDE,
};

static shell_screen_e g_screen;
static sw_timer_t g_timer;
static bool g_is_drawn;
static bool g_has_logo_appeared;
static uint32_t g_last_score;
static bool g_was_last_score_a_high_score;

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
    item->sprite = (uint8_t)in_sprite;
    item->palette = (uint8_t)in_palette;
    item->reserved = 0U;
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

static void prv_draw_menu(void)
{
    msg_display_list_t list = {0};

    render_init();

    prv_draw_centred_text(&list, MENU_TITLE_Y, TITLE_TEXT);
    prv_draw_title_actors(&list, MENU_ACTORS_Y);
    prv_draw_centred_text(&list, MENU_HEADING_Y, "HIGH SCORES");

    for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        /* One place digit, a space, then the score: eleven columns, centred as a block so
         * the three rows line up under each other whatever they hold. */
        const int16_t x = prv_get_centred_x((uint8_t)(PLACE_DIGITS + 2U + SCORE_DIGITS));
        const int16_t y = (int16_t)(MENU_FIRST_SCORE_Y + (place * MENU_SCORE_PITCH));

        prv_draw_number(&list, x, y, (uint32_t)(place + 1U), PLACE_DIGITS);
        prv_draw_number(&list, (int16_t)(x + ((PLACE_DIGITS + 2U) * GLYPH_SIZE)), y, high_score_get(place),
                        SCORE_DIGITS);
    }

    prv_draw_centred_text(&list, MENU_PROMPT_Y, "PRESS START");
    prv_flush(&list);
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
    g_last_score = game_session_get_score();
    g_was_last_score_a_high_score = high_score_offer(g_last_score);

    prv_enter(SHELL_SCREEN_SCORE, SHELL_SCORE_MS);
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
                prv_enter(SHELL_SCREEN_MENU, 0U);
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
        /* `game_session_init` re-initialises Render, which is what clears the menu off the
         * panel and gives the field handover a clean buffer to draw into. */
        game_session_init();
        game_session_start();

        prv_enter(SHELL_SCREEN_GAME, 0U);
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

shell_screen_e shell_get_screen(void)
{
    return g_screen;
}
