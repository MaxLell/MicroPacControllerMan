/*
 * game.h
 *
 * The game: it owns the Model, advances the tick, and decides what the tick meant
 * ([10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)/§10.5/§10.7/§10.9,
 * FR-006/007/011/017..021/024..027).
 *
 * It also owns the **game-internal broker** ([03 §3.6](../../../Docu/PrePlanning/03-Architecture.md),
 * FR-110) and publishes what happened onto it, which is how Score learns anything.
 *
 * Resolving a tick is deliberately synchronous. §10.7 requires collisions to be settled
 * *in the same tick as the movement*, including the case where Pacman and a ghost swap
 * cells and pass through each other — so movement, eating and collision cannot be spread
 * across bus round-trips without either a tick of latency or losing that case entirely.
 * The bus carries the resulting *events*, not the steps.
 *
 * Time comes in as elapsed milliseconds rather than being read from a clock, so a test can
 * drive years of play in an instant and the same code runs on the host and the target.
 */

#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "difficulty.h"
#include "ghost.h"
#include "msg.h"
#include "msg_broker.h"
#include "pacman.h"
#include "playfield.h"
#include "score.h"

/* ==========================================================================
 * game - public types
 * ========================================================================= */

/*! \brief Lives a run starts with (FR-006, §10.8). */
#define GAME_STARTING_LIVES                  (3U)

/*! \brief Internal broker depth. A tick can produce at most one pellet, four ghosts eaten
 *         and one frightened start, so this has generous headroom. */
#define GAME_INTERNAL_BROKER_CAPACITY        (16U)

/*! \brief Half a flash of the frightened warning (§10.9).
 *
 * A flash is one dark half and one light half, so the warning lasts
 * `frightened_flash_count * 2 * ` this. The arcade counts it in frames; a quarter of a
 * second per half reads the same and survives a frame rate that is not 60 exactly. */
#define GAME_FRIGHTENED_FLASH_HALF_PERIOD_MS (250U)

typedef enum
{
    GAME_STATE_IDLE = 0, /*!< Not started, or the run has ended  */
    GAME_STATE_RUNNING,
    GAME_STATE_OVER, /*!< All lives lost (FR-007)            */
    GAME_STATE_WON   /*!< Final level cleared (FR-027)       */
} game_state_e;

typedef struct
{
    /* --- the Model --- */
    playfield_t playfield;
    pacman_t pacman;
    ghost_t ghosts[GHOST_COUNT];
    score_t score;

    /* --- the run --- */
    game_state_e state;
    uint8_t level;
    uint8_t lives;

    /*! \brief What this level plays like, looked up once when it loads (§10.9). */
    difficulty_t difficulty;

    /* --- timing, all in milliseconds --- */
    uint32_t pacman_move_elapsed_ms;

    /*! \brief One accumulator per ghost, because they no longer share a speed: Blinky
     *         gets faster as the maze empties and any of them crawls in the tunnel. */
    uint32_t ghost_move_elapsed_ms[GHOST_COUNT];

    /*! \brief Whether Pacman's last step ended on a pellet — he takes the next one more
     *         slowly (§10.9). It is why a cleared corridor is an escape route. */
    bool did_pacman_eat_last_step;

    /*! \brief Whether the last step actually changed a cell, per actor.
     *
     * Read only by the interpolation. An entity stopped against a wall keeps its facing
     * and its timer keeps running (§10.1); without this the view would slide it in from
     * the cell behind it once per period, on the spot. */
    bool did_pacman_move;
    bool did_ghost_move[GHOST_COUNT];

    /*! \brief The controls that decide when Pinky, Inky and Clyde may leave the house
     *         (§10.4). Blinky never waits — he starts outside.
     *
     * Two counting schemes, and only one runs at a time. Normally each waiting ghost has its
     * own counter and only the most-preferred one is counting; after a life is lost the
     * personal ones are set aside for a **global** counter that releases at fixed totals.
     * The arcade does it this way so that a fresh start after a death is paced the same
     * however far into the level it happened. */
    uint16_t ghost_dot_counter[GHOST_COUNT];
    uint16_t global_dot_counter;
    bool is_global_dot_counter_active;

    /*! \brief How long since Pacman last ate. Runs the ghosts out of the house on its own,
     *         so standing still cannot keep them locked up. */
    uint32_t dots_idle_ms;

    uint32_t frightened_remaining_ms;
    uint32_t phase_remaining_ms;
    uint8_t phase_index; /*!< Position in the scatter/chase plan */

    /* --- the internal bus (FR-110) --- */
    msg_broker_t internal_broker;
    msg_t internal_msg_buffer[GAME_INTERNAL_BROKER_CAPACITY];

} game_t;

/* ==========================================================================
 * game - public API
 * ========================================================================= */

/*! \brief Wire the game up: internal broker, Score, and an idle state.
 *
 * Must be called once before anything else. #game_start begins an actual run.
 *
 * \param[out]      inout_game: instance to initialize, must not be `NULL`
 */
void game_init(game_t* inout_game);

/*! \brief Begin a new run at level 1 with a full set of lives (FR-003/006). */
void game_start(game_t* inout_game);

/*! \brief Record the player's intended direction (FR-004).
 *
 * Ignored unless a run is in progress. Takes effect at Pacman's next move, not now.
 */
void game_set_direction(game_t* inout_game, direction_e in_direction);

/*! \brief Advance the game by a slice of time.
 *
 * Each entity moves when its own period has elapsed (§10.1), so the caller only has to
 * report how much time passed. Mode timers, collisions and level completion are settled
 * inside. Does nothing unless a run is in progress.
 *
 * \param[in,out]   inout_game: initialized instance
 * \param[in]       in_elapsed_ms: milliseconds since the last call
 */
void game_tick(game_t* inout_game, uint32_t in_elapsed_ms);

/*! \brief Describe the world as the view needs to see it, **by value**.
 *
 * Not a handle into the game's memory: the caller gets a copy, which is what lets this
 * travel as an ordinary `MSG_GAME_STATE` payload and what removes the double buffer the
 * old snapshot scheme needed ([DEC-016](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).
 *
 * Call it at the **frame** rate, not the tick rate. Each actor's `progress` is computed
 * fresh from how far its movement period has run, so calling more often is what produces
 * smooth motion rather than more of the same picture ([10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)).
 *
 * \param[in]       in_game: the game, must not be `NULL`
 * \param[out]      out_state: filled with the current state, must not be `NULL`
 */
void game_get_state_message(const game_t* in_game, msg_game_state_t* out_state);

/*! \brief How the run stands. */
game_state_e game_get_state(const game_t* in_game);

/*! \brief The score so far, cumulative across levels (§10.9). */
uint32_t game_get_score(const game_t* in_game);

/*! \brief Lives left (FR-024). */
uint8_t game_get_lives(const game_t* in_game);

/*! \brief The level being played, `1`..#DIFFICULTY_FINAL_LEVEL (FR-025). */
uint8_t game_get_level(const game_t* in_game);

/*! \brief Whether ghosts are currently edible (§10.5). */
bool game_is_frightened_active(const game_t* in_game);

#endif /* GAME_H */
