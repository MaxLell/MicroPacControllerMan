/*
 * game_session.h
 *
 * One run of the game, paced and drawn: the loop body that turns `game`, `game_view` and
 * `render` into something a player sees ([03 §3.6](../../../Docu/PrePlanning/03-Architecture.md)).
 *
 * It exists because three callers need exactly the same frame — the target's
 * `app_main`, the host application, and the `pacman` on-target test — and the frame has
 * two traps in it that are not obvious from the outside: the pacing timer is one-shot and
 * has to re-arm itself, and a level change hands the whole field over across several
 * display lists rather than one. Both were paid for once already; a copy per caller would
 * have to pay again, and the on-target test would then be testing the copy.
 *
 * What it deliberately does **not** own is input or reporting. The three callers read three
 * different devices — a joystick, a keyboard, a joystick plus a confirm button — and say
 * different things about what they see, so the session takes a direction and answers
 * questions about the run, and leaves the talking to whoever is running it.
 *
 * A single run at a time, held in file-scope state rather than handed out as an object.
 * There is one panel and one frame buffer behind it (DEC-021), so a second session would
 * have nowhere to draw.
 */

#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"
#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * game_session - public types
 * ========================================================================= */

/*! \brief The frame period, in milliseconds — 60 FPS rounded to the 1 ms tick.
 *
 * M2 measured five moving actors at 5.26 ms of this budget with 20 x 20 sprites, and they
 * are 16 x 16 now. Public because it is also the simulation's time step: a caller that
 * reports timings is talking about this number.
 */
#define GAME_SESSION_FRAME_PERIOD_MS (16U)

/* ==========================================================================
 * game_session - public API
 * ========================================================================= */

/*! \brief Bring up the frame path and start the pacing timer.
 *
 * Initialises `render`, the game and the view, in that order. Expects the platform to be
 * up already — `sw_timer_init` and whatever the display needs — because the session knows
 * nothing about hardware.
 *
 * Does not start a run; call #game_session_start for that.
 */
void game_session_init(void);

/*! \brief Begin a run: three lives, level one, score zero (FR-003). */
void game_session_start(uint32_t in_maze_seed);

/*! \brief Begin a run on one given maze, which every level of it then plays.
 *
 * For a caller that needs a maze it can predict — a test, or anything that wants the arcade's
 * own layout (`playfield_get_arcade_map`).
 *
 * \param[in]       in_map: the maze to play, must not be `NULL`
 */
void game_session_start_on_map(const playfield_map_t* in_map);

/*! \brief Begin a run on the normal maze — the arcade's own layout, every level (FR-040).
 *
 * One of the two games the menu offers, and the only one the AI may be handed control in. Takes no
 * map on purpose: see #game_start_on_normal_maze for why nobody should be holding one.
 */
void game_session_start_on_normal_maze(void);

/*! \brief Ask for a direction.
 *
 * A request rather than a move: the turn happens at the first cell where it becomes
 * possible (§10.1), which is why a held stick has to keep asking.
 *
 * **Ignored while the AI is playing** (FR-031). Exclusivity lives here rather than in each of the
 * three callers, because this is the one door a direction comes through — so it holds however many
 * devices are wired to it.
 *
 * \param[in]       in_direction: the way the player is pushing
 */
void game_session_set_direction(direction_e in_direction);

/*! \brief Who is steering Pacman.
 *
 * Two of these are machines and they are *different* machines, which is the whole reason this is
 * an enumeration and not the flag it replaced, and it is kept as one although there is only one
 * machine left: the trained network went in DEC-054, and what the type says — *which* player, not
 * *whether* a machine plays — is still the right question for a caller to answer.
 */
typedef enum
{
    GAME_SESSION_PLAYER_HUMAN = 0, /*!< The joystick                                             */
    GAME_SESSION_PLAYER_LOOKAHEAD  /*!< The search of `pacman_lookahead` (DEC-050)               */
} game_session_player_e;

/*! \brief Hand Pacman to one of the machines, or take him back (FR-030).
 *
 * Refused, and reported as refused, when the player asked for cannot play: the generated weight
 * table may fail to evaluate, and the search needs a run in progress to clone. Better a run that
 * stays under the player's control than one where the stick is dead and nothing plays.
 *
 * The choice is the *run's*, so it survives a level change and a lost life (FR-033) — neither of
 * those goes near it. #game_session_start resets it, which is what makes every new run start under
 * player control.
 *
 * \param[in]       in_player: who should be steering
 * \return          `true` when control is now where the caller asked for it
 */
bool game_session_set_player(game_session_player_e in_player);

/*! \brief Who is steering Pacman right now. */
game_session_player_e game_session_get_player(void);

/*! \brief Hand Pacman to the machine, or take him back.
 *
 * #game_session_set_player said as a yes or no, kept because most callers only care whether a
 * *person* is playing — FR-034's high-score lockout, the joystick door, the on-target tests. It
 * cannot select the search; that needs the enumeration.
 *
 * \param[in]       in_is_enabled: `true` to let the machine play
 * \return          `true` when control is now where the caller asked for it
 */
bool game_session_set_ai_enabled(bool in_is_enabled);

/*! \brief Whether *any* machine is playing right now, network or search.
 *
 * Deliberately not "is the network playing". Everything that reads this is asking the FR-034
 * question — did a person earn this score — and a search earns it no more than a network does. */
bool game_session_is_ai_enabled(void);

/*! \brief Tell the frame whether the endless mode is on, so the HUD can say so (FR-043).
 *
 * The session has no opinion about the loop — it does not restart anything — and passes this to the
 * view. Whether a finished run is followed by another is the shell's, because the shell is what
 * knows about screens.
 *
 * \param[in]       in_is_infinite: `true` while a finished run will be followed by another
 */
void game_session_set_infinite(bool in_is_infinite);

/*! \brief Advance and draw the game if the frame is due.
 *
 * Call as often as you like — this is the whole of the session's share of the super-loop.
 * It steps the simulation by #GAME_SESSION_FRAME_PERIOD_MS, hands the state to the view,
 * drains every display list the view has (a level change produces several) and presents
 * the result.
 *
 * The caller still owns `sw_timer_process`, because the caller's loop may have timers of
 * its own.
 *
 * \return          `true` when a frame was drawn, so a caller can hang its own
 *                      once-per-frame work off the same call
 */
bool game_session_service(void);

/*! \brief Where the run has got to — running, over, or won (FR-003/027). */
game_state_e game_session_get_state(void);

/*! \brief Points scored so far. */
uint32_t game_session_get_score(void);

/*! \brief Lives left, the current one included. */
uint8_t game_session_get_lives(void);

/*! \brief The level being played, counting from 1. */
uint8_t game_session_get_level(void);

/*! \brief What the run currently looks like — the same message the view is drawn from.
 *
 * For a caller that has to *report* or *record* the run rather than draw it: the on-target AI test
 * says where Pacman is and which way the agent sent him, and a recorded state is what FR-039's
 * equivalence check replays. The four scalar getters above answer the common questions more
 * cheaply; this is the whole picture.
 *
 * \param[out]      out_state: filled in, must not be `NULL`
 */
void game_session_get_state_message(msg_game_state_t* out_state);

#endif /* GAME_SESSION_H */
