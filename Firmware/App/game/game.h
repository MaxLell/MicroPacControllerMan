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
#define GAME_STARTING_LIVES (3U)

/*! \brief Pacman's movement period, constant across levels (§10.1/§10.9). */
#define GAME_PACMAN_MOVE_PERIOD_MS (150U)

/*! \brief Internal broker depth. A tick can produce at most one pellet, four ghosts eaten
 *         and one frightened start, so this has generous headroom. */
#define GAME_INTERNAL_BROKER_CAPACITY (16U)

typedef enum
{
    GAME_STATE_IDLE = 0,                        /*!< Not started, or the run has ended  */
    GAME_STATE_RUNNING,
    GAME_STATE_OVER,                            /*!< All lives lost (FR-007)            */
    GAME_STATE_WON                              /*!< Final level cleared (FR-027)       */
} game_state_e;

/*! \brief What Render needs to draw a frame (§3.6, R-007).
 *
 * A description of the world, not pixels: the View turns it into a picture. Handed on as a
 * version plus a handle to one of two buffers the game swaps between, so the reader is
 * never looking at a frame being written.
 */
typedef struct
{
    uint32_t version;
    uint8_t level;
    uint8_t lives;
    uint32_t score;
    game_state_e state;
    bool is_frightened_active;
    cell_t pacman_cell;
    direction_e pacman_direction;
    cell_t ghost_cells[GHOST_COUNT];
    bool ghost_is_frightened[GHOST_COUNT];
    playfield_pellet_e pellets[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
} game_snapshot_t;

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

    /* --- timing, all in milliseconds --- */
    uint32_t pacman_move_elapsed_ms;
    uint32_t ghost_move_elapsed_ms;
    uint32_t frightened_remaining_ms;
    uint32_t phase_remaining_ms;
    uint8_t phase_index;                        /*!< Position in the scatter/chase plan */

    /* --- the internal bus (FR-110) --- */
    msg_broker_t internal_broker;
    msg_t internal_msg_buffer[GAME_INTERNAL_BROKER_CAPACITY];

    /* --- the published frame, double-buffered (R-007) --- */
    game_snapshot_t snapshots[2];
    uint8_t front_snapshot_index;
    uint32_t snapshot_version;
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

/*! \brief The current frame to draw.
 *
 * Refreshed by #game_tick. Valid until the next tick, which is why it is double-buffered:
 * the caller can draw from it while the next one is assembled.
 *
 * \return          The frame, never `NULL`
 */
const game_snapshot_t* game_get_snapshot(const game_t* in_game);

/*! \brief How the run stands. */
game_state_e game_get_state(const game_t* in_game);

/*! \brief The score so far, cumulative across levels (§10.9). */
uint32_t game_get_score(const game_t* in_game);

/*! \brief Lives left (FR-024). */
uint8_t game_get_lives(const game_t* in_game);

/*! \brief The level being played, `1`..#PLAYFIELD_LEVEL_COUNT (FR-025). */
uint8_t game_get_level(const game_t* in_game);

/*! \brief Whether ghosts are currently edible (§10.5). */
bool game_is_frightened_active(const game_t* in_game);

#endif /* GAME_H */
