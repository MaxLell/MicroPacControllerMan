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

/*! \brief Which game the menu offers (FR-040).
 *
 * Three different games rather than three settings, which is why each keeps its own high-score
 * table (FR-041) — the numbers are not comparable. The arcade's own maze is a maze that can be
 * learned; a generated one cannot be; and a run the agent played from the first frame is not a run
 * anybody played.
 *
 * The order is the order the menu lists them in, and the value is also the index of that game's
 * high-score table.
 */
typedef enum
{
    SHELL_MODE_NORMAL_MAZE = 0, /*!< The arcade's own layout, every level; the player may hand over
                                 *   to the AI mid-run and take Pac-Man back (FR-030)          */
    SHELL_MODE_AI,              /*!< The agent plays the normal maze on its own, start to finish;
                                 *   there is no taking over (FR-042)                          */
    SHELL_MODE_RANDOM_MAZE,     /*!< A maze generated for each level (FR-029)                   */
    SHELL_MODE_COUNT
} shell_mode_e;

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

/*! \brief Which agent the AI game will hand Pac-Man to, as a `game_session_player_e`.
 *
 * Moved by pushing the menu's cursor left or right while `PAC-MAN AI` is selected, and reported
 * here so a test or the console can say what the menu is showing without reading pixels. */
uint8_t shell_get_selected_ai(void);

/*! \brief Which game the menu is offering to start, and which one a run in progress is (FR-040).
 *
 * One value, not two: the selection can only be moved on the menu, so while a run is in progress
 * it still says what that run was started as. That is what makes it the right thing for
 * #shell_toggle_ai and the high-score table to be chosen by.
 */
shell_mode_e shell_get_selected_mode(void);

/*! \brief The name of a game, for a caller that reports what the board is doing.
 *
 * \param[in]       in_mode: a \ref shell_mode_e below #SHELL_MODE_COUNT
 * \return          A static string, never `NULL`
 */
const char* shell_get_mode_name(shell_mode_e in_mode);

/*! \brief Which screen is up. */
shell_screen_e shell_get_screen(void);

/*! \brief The board button, wherever it came from — and the shell decides what it means.
 *
 * One entry point rather than a caller working out the meaning, because the meaning depends on the
 * screen and on the game, and both of those are this module's:
 *
 * | screen | game | what the button does |
 * |---|---|---|
 * | menu, score | any | start (FR-003) |
 * | a run | normal maze | hand Pac-Man to the agent, or take him back (FR-030) |
 * | a run | Pac-Man AI | turn the endless loop on or off (FR-043) |
 * | a run | random maze | nothing — the agent is not offered there (FR-040) |
 *
 * The caller's job is to report the press and nothing else. #shell_toggle_ai and
 * #shell_toggle_infinite stay public for a test that means one of them specifically.
 */
void shell_press_user_button(void);

/*! \brief Hand Pac-Man between the player and the trained agent (FR-030).
 *
 * Only in a **normal-maze** run. Not on the menu or the score screen, where the same button means
 * start (FR-003); not in a random-maze run, where the agent is not offered (FR-040) and a menu that
 * says so must be told the truth by the button; and **not in the Pac-Man AI game**, where the agent
 * plays start to finish and there is nobody to hand back to (FR-042) — a game that let the player
 * take over would be the normal maze under another name. Also does nothing when the weight table
 * cannot be evaluated, which is reported rather than swallowed so a caller can say so.
 *
 * \return          `true` when control actually changed hands
 */
bool shell_toggle_ai(void);

/*! \brief Whether the agent is playing right now, for a caller that reports what the board does. */
bool shell_is_ai_playing(void);

/*! \brief Whether the AI played at any point in the current run — the FR-034 lockout.
 *
 * Sticky: handing control back does not clear it, only a new run does. Public so that an on-target
 * test can see the reason a high score was refused rather than having to infer it.
 */
bool shell_has_ai_played(void);

/*! \brief Turn the endless loop on or off (FR-043).
 *
 * Only in a **Pac-Man AI** run: while it is on, a run that ends starts the next one instead of
 * returning to the menu, so the agent keeps playing until somebody stops it. It belongs to that
 * game alone — a loop in a game a person is playing would restart a run they had not asked to
 * replay, and the menu is how a person asks.
 *
 * Cleared when a run is started from the menu, so a game begun by hand is one game. The loop's own
 * restarts do not clear it, which is what makes it a loop.
 *
 * \return          `true` when the loop was switched
 */
bool shell_toggle_infinite(void);

/*! \brief Whether the endless loop is on. */
bool shell_is_infinite(void);

/*! \brief How many runs the loop has played, the one in progress included; `0` outside it.
 *
 * For a caller that reports what the board is doing: an endless run of runs with no count is a
 * board that looks stuck rather than busy.
 */
uint32_t shell_get_run_count(void);

#endif /* SHELL_H */
