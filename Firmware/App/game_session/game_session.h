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

/*! \brief The frame period, in milliseconds — the 60 FPS of NFR-002 rounded to the 1 ms
 *         tick.
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

/*! \brief Ask for a direction.
 *
 * A request rather than a move: the turn happens at the first cell where it becomes
 * possible (§10.1), which is why a held stick has to keep asking.
 *
 * \param[in]       in_direction: the way the player is pushing
 */
void game_session_set_direction(direction_e in_direction);

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

#endif /* GAME_SESSION_H */
