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

#define GLYPH_SIZE        (8)
#define ACTOR_SIZE        (16)

/* The five actors of the title row, spaced so they do not touch. */
#define TITLE_ACTOR_COUNT (5)
#define TITLE_ACTOR_PITCH (24)

/* The game's own name, hyphen and all. It read `PACMAN` for a long time, because the font decoded
 * out of the tile ROM is the digits, the letters and a space — so this is the one string on the
 * panel that needed a glyph drawn rather than decoded (`SPRITE_SET_GLYPH_HYPHEN`). A hyphen in an
 * 8 x 8 monospace font is not the invention the project avoids elsewhere; a logo bitmap would be. */
#define TITLE_TEXT        "PAC-MAN"

/* What wipes the title's row. Spaces rather than a rectangle, which is how every other row on these
 * screens is cleared; the assertion below is what keeps it the same width as the thing it erases. */
#define TITLE_BLANK       "       "

_Static_assert(sizeof(TITLE_BLANK) == sizeof(TITLE_TEXT), "the blank must cover the title exactly");

#define SCORE_DIGITS           (7U)
#define PLACE_DIGITS           (1U)

/* Where the word screens put their rows, in panel pixels. Written as named constants rather than
 * arithmetic in the drawing code, because a layout is read far more often than it is computed.
 *
 * The masthead is two rows and the gap between them: the gap rather than a second absolute row, so
 * the title and the cast travel as one block wherever a screen puts it. */
#define MASTHEAD_ACTORS_OFFSET (16)
#define MASTHEAD_HEIGHT        (MASTHEAD_ACTORS_OFFSET + ACTOR_SIZE)

/* Where each screen that carries the masthead puts the title row. Three screens, three rows, each
 * named rather than derived from the others: the owner placed them by eye and there is no
 * relationship between them for arithmetic to preserve.
 *
 * The loading screen **centres** the block, because it carries nothing else — it is a title card, and
 * that is computed so the only two things deciding it are the panel's height and the block's own. The
 * menu puts it low, in the middle of the space above the two options, and only on the **first** page:
 * once PLAY or AI has been chosen, the pages that follow are questions to be read and the masthead
 * has said what it had to say. The score screen keeps it at the top, above `GAME OVER`. */
#define LOADING_MASTHEAD_Y     ((FRAMEBUFFER_HEIGHT - MASTHEAD_HEIGHT) / 2)
#define MENU_MASTHEAD_Y        (72)
#define SCORE_MASTHEAD_Y       (32)

/* The menu's own rows: the scores, and the choice of game.
 *
 * They used to be placed so that the block from the heading to the last option was centred on the
 * panel; they sit two rows lower than that now, which is what the owner asked for while the masthead
 * was still above them on every page.
 *
 * **The heading shares its row with the masthead's title, and the first score with the masthead's
 * cast.** That is safe because the two are mutually exclusive — the first page has no table to head
 * and no scores to list, and no other page has the masthead — but it is the reason
 * #prv_draw_selection wipes the masthead before drawing these rows and draws it after them. */
#define MENU_HEADING_Y         (72)
#define MENU_FIRST_SCORE_Y     (96)
#define MENU_SCORE_PITCH       (16)
#define MENU_FIRST_OPTION_Y    (176)
#define MENU_OPTION_PITCH      (34)

/* Pacman marks the selection, one sprite's width plus a gap to the left of the label, and raised
 * so his 16 px sit centred on an 8 px row of text. */
#define MENU_CURSOR_GAP        (8)
#define MENU_CURSOR_RISE       ((ACTOR_SIZE - GLYPH_SIZE) / 2)

#define SCORE_HEADING_Y        (120)
#define SCORE_VALUE_Y          (152)
#define SCORE_NOTE_Y           (184)

/* The palette each actor of the title row is drawn in, in the order they stand. */
static const sprite_set_palette_e g_title_palettes[TITLE_ACTOR_COUNT] = {
    SPRITE_SET_PALETTE_PACMAN, SPRITE_SET_PALETTE_BLINKY, SPRITE_SET_PALETTE_PINKY,
    SPRITE_SET_PALETTE_INKY,   SPRITE_SET_PALETTE_CLYDE,
};

/*! \brief What each page of the menu lists.
 *
 * Two options a page throughout, which is why there is no length beside them. The words are upper
 * case because that is the font: the 1980 tile ROM's letters, digits, a space and the one hyphen that
 * had to be drawn by hand (DEC-026). **Nothing needs a hyphen any more** — DEC-055's rows used them
 * as arrows to say "this row has other values", and a page whose whole content is a list of choices
 * does not have to say so. */
static const char* const g_page_options[SHELL_MENU_PAGE_COUNT][SHELL_MENU_OPTIONS] = {
    {"PLAY", "AI"},
    {"CLASSIC", "RANDOM"},
    {"ENDLESS OFF", "ENDLESS ON"},
};

/*! \brief What the current combination is called, for the same caller.
 *
 * Player-major, and reporting only: the high-score tables are the two mazes (FR-041), so nothing
 * indexes by this. */
static const char* const g_mode_names[SHELL_PLAYER_COUNT][SHELL_MAZE_COUNT] = {
    {"play classic", "play random"},
    {"ai classic", "ai random"},
};

/* The mazes and their tables are the same set counted twice, so let the build say so rather than a
 * comment. */
_Static_assert((uint8_t)SHELL_MAZE_COUNT == HIGH_SCORE_TABLE_COUNT, "one high-score table per maze");

static shell_screen_e g_screen;
static sw_timer_t g_timer;
static bool g_is_drawn;
static bool g_has_logo_appeared;
static uint32_t g_last_score;
static bool g_was_last_score_a_high_score;

/* Which game the menu is offering, and — because the selection can only be moved on the menu —
 * which game the run in progress is. Also the index of the high-score table that game keeps. */
static shell_player_e g_selected_player;
static shell_maze_e g_selected_maze;

/*! \brief Which page is up and which of its two options is highlighted.
 *
 * The player and the maze above are *decided*; this is where the cursor is. They are separate because
 * a page can be left and come back to — the board button steps back — and what was decided on the way
 * in has to survive that. */
static shell_menu_page_e g_menu_page;
static uint8_t g_selected_option;

/*! \brief Which machine the agent's own game hands Pac-Man to (FR-042).
 *
 * A *menu* choice and not an in-game one: the run is the agent's from the first frame, so there is
 * no moment during it at which a player is deciding anything, and the button that would carry the
 * choice already means the endless mode (FR-043). It is also the honest place for it — which agent
 * is playing is a property of the run, and picking it before the run starts is what makes two runs
 * comparable. */

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

/*! \brief Whether the row labels on the panel are the current ones.
 *
 * Separate from #g_is_selection_drawn because the two have different costs and different causes.
 * Moving the cursor changes neither a row's text nor the scores, so it may redraw the cursor alone —
 * which is what keeps a move to the cost of one rectangle. Changing a row's *value* changes that
 * row's text, and on the player and maze rows the three scores with it. */
static bool g_are_rows_drawn;

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
 * logo that exists in the tile ROM. Drawn, or wiped when `in_is_visible` is false. */
static void prv_draw_title_actors(msg_display_list_t* inout_list, int16_t in_y, bool in_is_visible)
{
    static const sprite_set_id_e k_sprites[TITLE_ACTOR_COUNT] = {
        SPRITE_SET_PACMAN_WIDE_EAST, SPRITE_SET_GHOST_EAST_A, SPRITE_SET_GHOST_EAST_A,
        SPRITE_SET_GHOST_EAST_A,     SPRITE_SET_GHOST_EAST_A,
    };
    const int16_t width = (TITLE_ACTOR_COUNT * TITLE_ACTOR_PITCH) - (TITLE_ACTOR_PITCH - ACTOR_SIZE);
    const int16_t first_x = (int16_t)((FRAMEBUFFER_WIDTH - width) / 2);

    for (uint8_t index = 0U; index < TITLE_ACTOR_COUNT; ++index)
    {
        prv_add(inout_list, k_sprites[index], in_is_visible ? g_title_palettes[index] : SPRITE_SET_PALETTE_EMPTY,
                (int16_t)(first_x + (index * TITLE_ACTOR_PITCH)), in_y);
    }
}

/* `PAC-MAN` and the row of actors — the title and the cast together, because the actors are the
 * closest thing to a logo the tile ROM has.
 *
 * It is on the loading screen, the menu's first page and the score screen, and each of those places
 * it for itself. It once inherited its place on the menu from the loading screen having just drawn it
 * with nothing painting over it — which is not the same as being drawn, and stopped being true the
 * moment a screen cleared the panel.
 *
 * `in_is_visible` false **wipes** it: the same sprites in the empty palette, so what is erased has
 * exactly the footprint of what was drawn and cannot drift from it. That is the same idiom as a spent
 * life in the HUD, which is a blank Pac-Man rather than a hole of counted spaces. */
static void prv_draw_masthead(msg_display_list_t* inout_list, int16_t in_title_y, bool in_is_visible)
{
    prv_draw_centred_text(inout_list, in_title_y, in_is_visible ? TITLE_TEXT : TITLE_BLANK);
    prv_draw_title_actors(inout_list, (int16_t)(in_title_y + MASTHEAD_ACTORS_OFFSET), in_is_visible);
}

static void prv_draw_loading(void)
{
    msg_display_list_t list = {0};

    prv_draw_masthead(&list, LOADING_MASTHEAD_Y, true);
    prv_flush(&list);
}

/* What an option on the current page says. */
static const char* prv_get_option_label(uint8_t in_option)
{
    return g_page_options[g_menu_page][in_option];
}

static int16_t prv_get_option_y(uint8_t in_option)
{
    return (int16_t)(MENU_FIRST_OPTION_Y + ((int16_t)in_option * MENU_OPTION_PITCH));
}

/* Where a page's options start.
 *
 * **Left-aligned to each other, and the block of them centred**, which is what the owner asked for:
 * `PLAY` and `AI` begin at the same column rather than each being centred on its own and so starting
 * at different ones. The block's width is the page's longest option, so the page as a whole still
 * sits in the middle of the panel.
 *
 * Per page rather than per option, which is the whole point — and it is also what lets the cursor
 * stand at a fixed column while it moves up and down. */
static int16_t prv_get_page_x(void)
{
    uint8_t widest = 0U;

    for (uint8_t option = 0U; option < SHELL_MENU_OPTIONS; ++option)
    {
        const uint8_t length = (uint8_t)strlen(prv_get_option_label(option));

        if (length > widest)
        {
            widest = length;
        }
    }

    return prv_get_centred_x(widest);
}

static int16_t prv_get_cursor_x(void)
{
    return (int16_t)(prv_get_page_x() - ACTOR_SIZE - MENU_CURSOR_GAP);
}

/* Whether the page that is up shows a high-score table.
 *
 * Only the maze page, and only for a person: that is where a maze is highlighted, so there is a table
 * to show — and the machine has none (DEC-056). The player page has not chosen anything yet and the
 * endless page is not about a maze. */
static bool prv_does_page_show_scores(void)
{
    return (g_menu_page == SHELL_MENU_PAGE_MAZE) && (g_selected_player == SHELL_PLAYER_PERSON);
}

/* The three scores of the maze the cursor is on (FR-041), or blanks where there is no table.
 *
 * Blanked rather than skipped, for the reason a spent life is drawn as a blank: a row that stopped
 * being described would leave the last numbers it held on the panel, and on the AI's pages those
 * would be a person's scores under a machine's game. */
static void prv_draw_scores(msg_display_list_t* inout_list)
{
    const bool has_table = prv_does_page_show_scores();

    for (uint8_t place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        /* One place digit, a space, then the score: eleven columns, centred as a block so the three
         * rows line up under each other whatever they hold. */
        const int16_t x = prv_get_centred_x((uint8_t)(PLACE_DIGITS + 2U + SCORE_DIGITS));
        const int16_t y = (int16_t)(MENU_FIRST_SCORE_Y + (place * MENU_SCORE_PITCH));

        if (!has_table)
        {
            prv_draw_text(inout_list, x, y, "           ");

            continue;
        }

        prv_draw_number(inout_list, x, y, (uint32_t)(place + 1U), PLACE_DIGITS);
        prv_draw_number(inout_list, (int16_t)(x + ((PLACE_DIGITS + 2U) * GLYPH_SIZE)), y,
                        high_score_get((uint8_t)g_selected_option, place), SCORE_DIGITS);
    }
}

/* `HIGH SCORES` and the three numbers under it, or blanks where there is no table.
 *
 * The heading goes with the numbers rather than standing above the whole menu, because it is a label
 * *for them*: on the first page nothing has been chosen yet and on the endless page the question is
 * not about a maze, so a heading there would be announcing an empty space. The owner asked for it to
 * go from the first page, and the rule that follows from the reason covers the third as well. */
static void prv_draw_heading(msg_display_list_t* inout_list)
{
    prv_draw_centred_text(inout_list, MENU_HEADING_Y, "           ");

    if (prv_does_page_show_scores())
    {
        prv_draw_centred_text(inout_list, MENU_HEADING_Y, "HIGH SCORES");
    }
}

static void prv_draw_options(msg_display_list_t* inout_list)
{
    const int16_t x = prv_get_page_x();

    for (uint8_t option = 0U; option < SHELL_MENU_OPTIONS; ++option)
    {
        /* Blanked wider than any page's block and centred, so whatever the previous page left is
         * covered wherever it stood — a page's block moves when its longest option is a different
         * length, and a shorter one would otherwise leave the tail of a longer one behind. */
        prv_draw_centred_text(inout_list, prv_get_option_y(option), "             ");
        prv_draw_text(inout_list, x, prv_get_option_y(option), prv_get_option_label(option));
    }
}

/* What a change of page or cursor puts on the panel.
 *
 * The cursor goes **last and in a list of its own**, because Render restores the pixels under the
 * actors of the *previous* list — so a background list drawn after it would wipe it. */
static void prv_draw_selection(void)
{
    msg_display_list_t list = {0};
    const bool is_page_new = !g_are_rows_drawn;
    const bool has_masthead = (g_menu_page == SHELL_MENU_PAGE_PLAYER);

    if (is_page_new)
    {
        /* **The masthead is wiped before the rows and drawn after them**, and the asymmetry is the
         * whole reason this is written out rather than being one call: its title shares row
         * #MENU_MASTHEAD_Y with `HIGH SCORES`, and its cast shares the row below with the first
         * score. Wiped last it would eat the heading of the page it just left; drawn first it would
         * be eaten by the blanks of the page it belongs to. */
        if (!has_masthead)
        {
            prv_draw_masthead(&list, MENU_MASTHEAD_Y, false);
            prv_flush(&list);
        }

        /* The heading goes with the options and not with the scores, although it is a label for them:
         * whether there *is* a table depends on the page and on who is playing, which is exactly what
         * changes when the options do. Moving the cursor within a page changes which table is shown
         * and never whether one is, so redrawing the heading there would be a region a frame did not
         * need — and the test that holds a menu move to one rectangle caught it at 45 against 44. */
        prv_draw_heading(&list);
        prv_draw_options(&list);
        prv_flush(&list);

        g_are_rows_drawn = true;
    }

    /* The scores follow the cursor on the maze page — each maze has its own table — so they are
     * redrawn with it and not with the options. A page with no table has nothing that follows the
     * cursor: its blank rows erase what the previous page left, which is a page's business and not a
     * cursor's. Blanking them on every move would also wipe the masthead the first page carries. */
    if (prv_does_page_show_scores() || is_page_new)
    {
        prv_draw_scores(&list);
        prv_flush(&list);
    }

    if (is_page_new && has_masthead)
    {
        prv_draw_masthead(&list, MENU_MASTHEAD_Y, true);
        prv_flush(&list);
    }

    prv_add_cursor(&list, prv_get_cursor_x(), (int16_t)(prv_get_option_y(g_selected_option) - MENU_CURSOR_RISE));
    prv_flush(&list);

    g_is_selection_drawn = true;
}

/* Move to a page, with the cursor on whatever that page has already been told.
 *
 * A page can be left and come back to, so it opens on the choice that was made rather than at the
 * top: going back from the maze page and forward again must not silently move the maze. */
static void prv_open_page(shell_menu_page_e in_page)
{
    g_menu_page = in_page;

    switch (in_page)
    {
        case SHELL_MENU_PAGE_PLAYER: g_selected_option = (uint8_t)g_selected_player; break;
        case SHELL_MENU_PAGE_MAZE: g_selected_option = (uint8_t)g_selected_maze; break;
        default: g_selected_option = g_is_infinite ? 1U : 0U; break;
    }

    g_are_rows_drawn = false;
    g_is_selection_drawn = false;
}

static void prv_draw_menu(void)
{
    render_init();

    /* Nothing of the menu is drawn here, which is why there is no display list to draw it into: every
     * part of it can change without the screen being re-entered — the options with the page, the
     * heading and the scores with what is highlighted, the masthead with whether this is the first
     * page — so #prv_draw_selection owns all of it and this only clears the panel. */
    g_are_rows_drawn = false;

    prv_draw_selection();
}

static void prv_draw_score(void)
{
    msg_display_list_t list = {0};
    const bool has_won = (game_session_get_state() == GAME_STATE_WON);

    render_init();

    prv_draw_masthead(&list, SCORE_MASTHEAD_Y, true);
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
     * **There is no exception any more** (DEC-056): the machine's own tables are gone, so a run it
     * played is recorded nowhere at all. That is what FR-034 said before DEC-046 gave the agent one,
     * and it is the simpler rule — a run nobody played is not a score anybody set. */
    g_last_score = game_session_get_score();
    g_was_last_score_a_high_score = g_has_ai_played ? false : high_score_offer((uint8_t)g_selected_maze, g_last_score);

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

    if (g_selected_maze == SHELL_MAZE_RANDOM)
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
        game_session_start_on_normal_maze();
    }

    g_has_ai_played = false;

    if (g_selected_player == SHELL_PLAYER_MACHINE)
    {
        /* The search has Pac-Man from the first frame and keeps him (FR-042). It cannot refuse —
         * there is nothing about it to be unavailable, unlike the trained table this used to have to
         * check for (DEC-054) — so the return value is discarded deliberately rather than ignored. */
        (void)game_session_set_player(GAME_SESSION_PLAYER_LOOKAHEAD);

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
    g_selected_player = SHELL_PLAYER_PERSON;
    g_selected_maze = SHELL_MAZE_CLASSIC;
    g_menu_page = SHELL_MENU_PAGE_PLAYER;
    g_selected_option = 0U;
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
                if (g_is_infinite && (g_selected_player == SHELL_PLAYER_MACHINE) && prv_start_run())
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
        /* **The centre key takes the highlighted option, which is not always "start".** On every page
         * but the last of a path it moves on; only the last one begins a run. Which page is last
         * depends on who is playing, and that is the whole of why the endless page needs no rule about
         * when it applies: a person's path does not go through it (DEC-056). */
        if (g_menu_page == SHELL_MENU_PAGE_PLAYER)
        {
            prv_open_page(SHELL_MENU_PAGE_MAZE);

            return;
        }

        if ((g_menu_page == SHELL_MENU_PAGE_MAZE) && (g_selected_player == SHELL_PLAYER_MACHINE))
        {
            prv_open_page(SHELL_MENU_PAGE_ENDLESS);

            return;
        }

        /* **The loop is not cleared here.** It used to be, because it was switched on *during* a run
         * and a stale flag would have looped a game begun by hand (FR-043). It is a page of the menu
         * now, defaulting to off and looked at on the way in, so clearing it would throw away what the
         * player just asked for. What clears it is choosing a person on the first page. */
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
        /* Back at the first page: a run that has just ended is not a reason to be three pages deep in
         * the choices that started it. */
        prv_open_page(SHELL_MENU_PAGE_PLAYER);
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

    /* **Left is back**, which the owner asked for once the pages existed: the stick already points the
     * way the pages go, so the key that means "the other direction" should mean the other direction.
     * It does the same thing the board button does — one behaviour, two devices, and the rule lives in
     * #shell_press_back rather than being written twice.
     *
     * Right is deliberately *not* forward. Confirming is what the centre key is for, and a page whose
     * options are a list has nothing sideways to be; a key that quietly took the highlighted option
     * would make a nudged stick start a game. */
    if (in_direction == DIRECTION_WEST)
    {
        (void)shell_press_back();

        return;
    }

    if ((in_direction != DIRECTION_NORTH) && (in_direction != DIRECTION_SOUTH))
    {
        return;
    }

    /* Stops at the ends rather than wrapping: with two options, wrapping would make up and down do
     * the same thing and neither of them mean anything. */
    uint8_t moved = g_selected_option;

    if ((in_direction == DIRECTION_NORTH) && (g_selected_option > 0U))
    {
        moved = (uint8_t)(g_selected_option - 1U);
    }
    else if ((in_direction == DIRECTION_SOUTH) && (g_selected_option < (SHELL_MENU_OPTIONS - 1U)))
    {
        moved = (uint8_t)(g_selected_option + 1U);
    }
    else
    {
        return;
    }

    g_selected_option = moved;

    /* The highlighted option *is* the choice, so moving the cursor decides — there is no separate
     * commit within a page. What the centre key does is leave the page. */
    switch (g_menu_page)
    {
        case SHELL_MENU_PAGE_PLAYER:
            g_selected_player = (shell_player_e)moved;

            /* A person's path never reaches the endless page, so a loop left armed by a previous trip
         * through the machine's would be on with nobody able to see or switch it. */
            if (g_selected_player == SHELL_PLAYER_PERSON)
            {
                g_is_infinite = false;

                game_session_set_infinite(false);
            }
            break;
        case SHELL_MENU_PAGE_MAZE: g_selected_maze = (shell_maze_e)moved; break;
        default:
            g_is_infinite = (moved != 0U);

            game_session_set_infinite(g_is_infinite);
            break;
    }

    /* Redrawn by #shell_service and not here, because a caller that blits the frame buffer into a
     * window does it when the service call says something reached the panel. */
    g_is_selection_drawn = false;
}

bool shell_press_back(void)
{
    if ((g_screen != SHELL_SCREEN_MENU) || (g_menu_page == SHELL_MENU_PAGE_PLAYER))
    {
        return false;
    }

    prv_open_page((shell_menu_page_e)(g_menu_page - 1U));

    return true;
}

shell_player_e shell_get_selected_player(void)
{
    return g_selected_player;
}

shell_maze_e shell_get_selected_maze(void)
{
    return g_selected_maze;
}

shell_menu_page_e shell_get_menu_page(void)
{
    return g_menu_page;
}

uint8_t shell_get_selected_option(void)
{
    return g_selected_option;
}

const char* shell_get_mode_name(void)
{
    return g_mode_names[g_selected_player][g_selected_maze];
}

void shell_press_user_button(void)
{
    /* **Back, where there is anywhere to go back to; start otherwise.** The menu is pages deep since
     * DEC-056, so picking `AI` by accident needs a way out — every other key goes forwards. On the
     * first page and on the score screen there is nothing to leave, and the button falls through to
     * meaning start, which is what keeps `button` on the console worth having. During a run it means
     * nothing: `shell_press_start` knows a run in progress has no start to press. */
    if (shell_press_back())
    {
        return;
    }

    shell_press_start();
}

bool shell_is_ai_playing(void)
{
    return (g_screen == SHELL_SCREEN_GAME) && game_session_is_ai_enabled();
}

bool shell_has_ai_played(void)
{
    return g_has_ai_played;
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
