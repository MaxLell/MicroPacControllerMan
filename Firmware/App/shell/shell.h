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
 * It owns the flow and the two screens made of words; it does not own **input** or
 * **reporting**, for the same reason `game_session` does not. The target reads a joystick
 * and a button, the host reads a keyboard, and they say different things about what they
 * see — so both call #shell_press_start and #shell_set_direction and do their own talking.
 *
 * Everything it draws is the arcade's own material: the title is set in the tile ROM's font
 * and the row beneath it is Pacman and the four ghosts. There is no logo bitmap in the ROM
 * to decode, and drawing one would be the invention this project has been avoiding.
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
 * Starts a run from the menu, and cuts the score screen short — a player who is ready does
 * not want to be told to wait two seconds. Ignored while a run is in progress and during
 * the loading screen, which nothing should be able to skip past: NFR-001 makes it short
 * enough already.
 */
void shell_press_start(void);

/*! \brief Ask for a direction; ignored unless a run is in progress. */
void shell_set_direction(direction_e in_direction);

/*! \brief Which screen is up. */
shell_screen_e shell_get_screen(void);

#endif /* SHELL_H */
