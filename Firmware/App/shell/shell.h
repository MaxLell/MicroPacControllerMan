/*
 * shell.h
 *
 * The screens around the game, and the order they come in: loading, menu, game, score,
 * menu again (FR-001, FR-002, FR-003, FR-023).
 *
 * The game is one state of this machine rather than the thing the firmware does. That is
 * the difference between a demo and a product: a run has to *end* somewhere, the score has
 * to be shown and offered to the table, and the board has to be ready for the next player
 * without being reset.
 *
 * It also owns **which game a player asked for** (FR-040): the menu offers the two mazes and
 * the run is started on the chosen one. That belongs here rather than in `game_session`
 * because it is a *choice made on a screen*, and the screens are what this module is.
 *
 * It owns the flow and the two screens made of words; it does not own **input** or
 * **reporting**, for the same reason `game_session` does not. The target reads a joystick
 * and a button, the host reads a keyboard, and they say different things about what they
 * see — so both call #shell_press_start, #shell_move_selection and #shell_set_direction and
 * do their own talking.
 *
 * Everything it draws is the arcade's own material: the loading screen's title is set in the
 * tile ROM's font, the row beneath it is Pacman and the four ghosts, and the menu marks what
 * is selected with Pacman himself. There is no logo bitmap in the ROM to decode, and drawing
 * one would be the invention this project has been avoiding.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"

/* ==========================================================================
 * shell - public types
 * ========================================================================= */

/*! \brief Where the machine is. */
typedef enum
{
    SHELL_SCREEN_LOADING = 0, /*!< The title, on its own, at power-on (FR-001)       */
    SHELL_SCREEN_MENU,        /*!< The three best scores, waiting for a player (FR-002) */
    SHELL_SCREEN_GAME,        /*!< A run in progress                                 */
    SHELL_SCREEN_SCORE        /*!< What the run came to, briefly (FR-023)            */
} shell_screen_e;

/*! \brief Who steers Pac-Man in the run the menu is offering (FR-040). */
typedef enum
{
    SHELL_PLAYER_PERSON = 0, /*!< The joystick                                     */
    SHELL_PLAYER_MACHINE,    /*!< The look-ahead search, start to finish (FR-042)   */
    SHELL_PLAYER_COUNT
} shell_player_e;

/*! \brief Which maze the run plays — and, for a run a person plays, its high-score table (FR-041).
 *
 * **Two tables, not four.** DEC-056 took the machine's away at the owner's request: a run nobody
 * played is not a score anybody set, so it is recorded nowhere at all. That is what FR-034 said
 * before DEC-046 gave the agent a table of its own, and this is the same answer arrived at again.
 */
typedef enum
{
    SHELL_MAZE_CLASSIC = 0, /*!< The arcade's own layout, at every level          */
    SHELL_MAZE_RANDOM,      /*!< A maze generated for each level (FR-029)          */
    SHELL_MAZE_COUNT
} shell_maze_e;

/*! \brief Which page of the menu is up.
 *
 * **A list and a confirm, three pages deep**, which is DEC-056's shape and the third the menu has
 * had: one screen of three fixed games, then two axes on one screen, then this. Each page is a short
 * list, up and down move within it, and the centre key takes the highlighted one — which either
 * advances to the next page or starts the run.
 *
 * The pages a run passes through depend on who is playing: a person picks a maze and goes, and the
 * machine picks a maze and then whether to loop. So \ref SHELL_MENU_PAGE_ENDLESS is only ever
 * reached from \ref SHELL_PLAYER_MACHINE, which is why the endless mode needs no rule of its own
 * about when it applies — it cannot be reached where it would not mean anything.
 */
typedef enum
{
    SHELL_MENU_PAGE_PLAYER = 0, /*!< `PLAY` / `AI`                                  */
    SHELL_MENU_PAGE_MAZE,       /*!< `CLASSIC` / `RANDOM`                           */
    SHELL_MENU_PAGE_ENDLESS,    /*!< `ENDLESS OFF` / `ENDLESS ON`, the machine only */
    SHELL_MENU_PAGE_COUNT
} shell_menu_page_e;

/*! \brief How many options any one page lists. Two throughout. */
#define SHELL_MENU_OPTIONS  (2U)

/*! \brief How long the panel stays dark before the title appears (NFR-005). */
#define SHELL_LOGO_DELAY_MS (200U)

/*! \brief How long the loading screen lasts in total, the delay included (NFR-001). */
#define SHELL_LOADING_MS    (3000U)

/*! \brief How long the score screen stays up before the menu returns (FR-023). */
#define SHELL_SCORE_MS      (2000U)

/* ==========================================================================
 * shell - public API
 * ========================================================================= */

/*! \brief Start at the loading screen.
 *
 * Expects the platform up — `sw_timer`, the display, and `high_score` loaded — because the
 * menu shows the stored scores the moment it appears.
 */
void shell_init(void);

/*! \brief Advance the flow, and run a game frame if one is due.
 *
 * The whole of the shell's share of the super-loop. The caller still owns
 * `sw_timer_process`, because the caller's loop may have timers of its own.
 *
 * \return          `true` when something reached the panel — a game frame, or a screen of
 *                      words drawn as it came up. That is the question a host application
 *                      blitting the frame buffer into a window has to ask, and it is a
 *                      superset of "was there a frame", so a caller hanging once-per-frame
 *                      work off it gets what it wanted too
 */
bool shell_service(void);

/*! \brief The start key, wherever it came from (FR-003).
 *
 * Starts the selected game from the menu, and cuts the score screen short — a player who is
 * ready does not want to be told to wait two seconds. Ignored while a run is in progress and
 * during the loading screen, which nothing should be able to skip past: NFR-001 makes it short
 * enough already.
 */
void shell_press_start(void);

/*! \brief Ask for a direction; ignored unless a run is in progress. */
void shell_set_direction(direction_e in_direction);

/*! \brief Move the menu's selection (FR-040).
 *
 * \ref DIRECTION_NORTH goes up the list and \ref DIRECTION_SOUTH down it; both stop at the end
 * rather than wrapping. Anything else, and any screen other than the menu, is ignored.
 *
 * Wants an **edge**, not a held stick: a caller that reports a held direction once per frame
 * would run the selection to the end of the list in a fraction of a second. That is the
 * caller's business rather than this module's, because only the caller knows what its device
 * does — which is the same reason #shell_set_direction wants the opposite.
 */
void shell_move_selection(direction_e in_direction);

/*! \brief Who the menu is offering to have play, and which maze it is offering.
 *
 * The selection can only be moved on the menu, so while a run is in progress these still say what
 * that run was started as — which is what makes the maze the right thing for the high-score table to
 * be chosen by (FR-041).
 */
shell_player_e shell_get_selected_player(void);
shell_maze_e shell_get_selected_maze(void);

/*! \brief Which page of the menu is up, and which of its options is highlighted.
 *
 * For a test or the console that has to say what the menu is showing without reading pixels.
 */
shell_menu_page_e shell_get_menu_page(void);
uint8_t shell_get_selected_option(void);

/*! \brief Take a step back through the menu's pages — what the board button means there.
 *
 * Without it, picking `AI` by accident is a trap: every other key goes forwards. On the first page
 * there is nowhere to go back to and nothing happens.
 *
 * \return          `true` when a page was actually left
 */
bool shell_press_back(void);

/*! \brief What the menu is offering, named, for a caller that reports what the board is doing.
 *
 * The four combinations of player and maze, for reporting only — the high-score tables are the two
 * mazes and nothing indexes by this.
 * \return          A static string, never `NULL`
 */
const char* shell_get_mode_name(void);

/*! \brief Which screen is up. */
shell_screen_e shell_get_screen(void);

/*! \brief The board button, wherever it came from — and the shell decides what it means.
 *
 * One entry point rather than a caller working out the meaning, because the meaning depends on the
 * screen and on the game, and both of those are this module's:
 *
 * | screen | what the button does |
 * |---|---|---|
 * | menu, score | start (FR-003) |
 * | a run | nothing |
 *
 * **It has one meaning left.** Handing Pac-Man over mid-run went in DEC-054 and the endless mode
 * moved onto the menu in DEC-055, so during a run there is nothing for it to do. The caller's job is
 * to report the press and nothing else.
 */
void shell_press_user_button(void);

/*! \brief Whether the agent is playing right now, for a caller that reports what the board does. */
bool shell_is_ai_playing(void);

/*! \brief Whether the AI played at any point in the current run — the FR-034 lockout.
 *
 * Sticky: handing control back does not clear it, only a new run does. Public so that an on-target
 * test can see the reason a high score was refused rather than having to infer it.
 */
bool shell_has_ai_played(void);

/*! \brief Whether the endless loop is on (FR-043).
 *
 * Set on the **menu** since DEC-055, on a row of its own that only appears while the machine plays:
 * a person's run has nothing to loop, and the board button that used to carry it now means one
 * thing everywhere. A finished run starts the next instead of returning to the menu, and the HUD
 * says `LOOP` while it is on. */
bool shell_is_infinite(void);

/*! \brief How many runs the loop has played, the one in progress included; `0` outside it.
 *
 * For a caller that reports what the board is doing: an endless run of runs with no count is a
 * board that looks stuck rather than busy.
 */
uint32_t shell_get_run_count(void);

#endif /* SHELL_H */
