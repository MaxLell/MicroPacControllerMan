#include "game.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "ghost.h"
#include "msg.h"
#include "msg_broker.h"
#include "pacman.h"
#include "playfield.h"
#include "score.h"

/* ==========================================================================
 * game - private
 * ========================================================================= */

/*! \brief Per-level difficulty, transcribed from §10.9.
 *
 * The scatter/chase plan is expressed as a repeat count rather than a list, which covers
 * every row of that table: `scatter_repeat_count` alternations of scatter-then-chase, then
 * chase for good. Level 5 has a count of zero, which is exactly "chase only". */
typedef struct
{
    uint32_t ghost_move_period_ms;
    uint32_t frightened_duration_ms;
    uint32_t scatter_duration_ms;
    uint32_t chase_duration_ms;
    uint8_t scatter_repeat_count;
} game_level_config_t;

static const game_level_config_t k_level_configs[PLAYFIELD_LEVEL_COUNT] = {
    {200U, 6000U, 5000U, 20000U, 2U},           /* level 1 — slower than Pacman        */
    {170U, 5000U, 4000U, 20000U, 2U},
    {150U, 4000U, 3000U, 20000U, 1U},           /* level 3 — matches Pacman's speed    */
    {130U, 2000U, 2000U, 20000U, 1U},
    {110U, 0U, 0U, 20000U, 0U}};                /* level 5 — faster, no frightened     */

/* §10.5: a frightened ghost moves at half its current speed. */
#define FRIGHTENED_SPEED_DIVISOR (2U)

static const game_level_config_t* prv_get_config(const game_t* const in_game)
{
    ASSERT(in_game->level >= PLAYFIELD_FIRST_LEVEL);
    ASSERT(in_game->level <= PLAYFIELD_LEVEL_COUNT);

    return &k_level_configs[in_game->level - PLAYFIELD_FIRST_LEVEL];
}

static void prv_publish(game_t* const inout_game, msg_id_e in_topic, const void* const in_payload,
                        uint16_t in_payload_size)
{
    msg_t msg = {0};
    msg_broker_status_e status;

    msg.id = in_topic;
    msg.payload_size = in_payload_size;

    if (in_payload != NULL)
    {
        memcpy(msg.payload, in_payload, in_payload_size);
    }

    /* Kept out of the ASSERT on purpose: an assertion compiles away under NDEBUG and would
     * take the publish with it. A full internal queue means the game produced more events in
     * one tick than the buffer holds, which is a sizing mistake, not a load condition. */
    status = msg_broker_publish(&inout_game->internal_broker, &msg);

    ASSERT(status == MSG_BROKER_STATUS_OK);
}

/* Build the internal bus and put Score on it.
 *
 * Broker and Score are wired together in one place because they cannot be renewed
 * separately: a subscriber may register for a topic exactly once, so a second #score_init on
 * a broker that already knows it would subscribe twice — and every pellet would then score
 * twice. Starting a run rebuilds both, which also throws away any event still queued from
 * the run before it. */
static void prv_init_bus(game_t* const inout_game)
{
    memset(&inout_game->internal_broker, 0, sizeof(inout_game->internal_broker));

    msg_broker_init(&inout_game->internal_broker, inout_game->internal_msg_buffer,
                    GAME_INTERNAL_BROKER_CAPACITY);
    score_init(&inout_game->score, &inout_game->internal_broker);
    msg_broker_start(&inout_game->internal_broker);
}

/* Move the events on to their subscribers and let them act. Called at the end of a tick, so
 * a handler never runs in the middle of resolving one. */
static void prv_deliver_events(game_t* const inout_game)
{
    (void)msg_broker_process_all(&inout_game->internal_broker);
    (void)score_process(&inout_game->score);
}

/* --- placement ----------------------------------------------------------- */

/* Put everyone back where the level starts, keeping score, lives and eaten pellets. Used
 * both after losing a life and when a new level loads (§10.7). */
static void prv_place_entities(game_t* const inout_game)
{
    pacman_reset(&inout_game->pacman, playfield_get_pacman_start(&inout_game->playfield));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        /* Three pen cells for four ghosts, so they share — they leave immediately anyway. */
        const cell_t pen_cell = playfield_get_pen_cell(&inout_game->playfield,
                                                       (uint8_t)(index % PLAYFIELD_PEN_CELL_COUNT));

        ghost_reset(&inout_game->ghosts[index], (ghost_personality_e)index, pen_cell);
    }

    inout_game->pacman_move_elapsed_ms = 0U;
    inout_game->ghost_move_elapsed_ms = 0U;
    inout_game->frightened_remaining_ms = 0U;
    inout_game->phase_index = 0U;
    inout_game->phase_remaining_ms = prv_get_config(inout_game)->scatter_duration_ms;
}

static void prv_load_level(game_t* const inout_game, uint8_t in_level)
{
    inout_game->level = in_level;

    playfield_load_level(&inout_game->playfield, in_level);
    prv_place_entities(inout_game);
}

/* --- scatter / chase / frightened ---------------------------------------- */

/* Which non-frightened mode the plan of §10.4 calls for right now. Even phases are scatter,
 * odd ones chase; once the repeats are used up it is chase for good. */
static ghost_mode_e prv_get_scheduled_mode(const game_t* const in_game)
{
    const game_level_config_t* const config = prv_get_config(in_game);

    if (in_game->phase_index >= (uint8_t)(config->scatter_repeat_count * 2U))
    {
        return GHOST_MODE_CHASE;
    }

    return ((in_game->phase_index % 2U) == 0U) ? GHOST_MODE_SCATTER : GHOST_MODE_CHASE;
}

static uint32_t prv_get_phase_duration(const game_t* const in_game)
{
    const game_level_config_t* const config = prv_get_config(in_game);

    return (prv_get_scheduled_mode(in_game) == GHOST_MODE_SCATTER) ? config->scatter_duration_ms
                                                                  : config->chase_duration_ms;
}

/* Push the mode every entity should be in. Safe to call every tick: setting a mode a ghost
 * is already in is a no-op and costs it no reversal (§10.1). */
static void prv_apply_mode(game_t* const inout_game)
{
    const bool is_frightened = inout_game->frightened_remaining_ms > 0U;
    const ghost_mode_e scheduled = prv_get_scheduled_mode(inout_game);

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_set_mode(&inout_game->ghosts[index], is_frightened ? GHOST_MODE_FRIGHTENED
                                                                 : scheduled);
    }
}

static void prv_advance_timers(game_t* const inout_game, uint32_t in_elapsed_ms)
{
    if (inout_game->frightened_remaining_ms > 0U)
    {
        /* The frightened window freezes the scatter/chase plan rather than running it down
         * in the background, so the ghosts resume the phase they were interrupted in. */
        inout_game->frightened_remaining_ms
            = (inout_game->frightened_remaining_ms > in_elapsed_ms)
                  ? (inout_game->frightened_remaining_ms - in_elapsed_ms)
                  : 0U;

        return;
    }

    if (inout_game->phase_remaining_ms > in_elapsed_ms)
    {
        inout_game->phase_remaining_ms -= in_elapsed_ms;

        return;
    }

    ++inout_game->phase_index;
    inout_game->phase_remaining_ms = prv_get_phase_duration(inout_game);
}

static void prv_start_frightened(game_t* const inout_game)
{
    const uint32_t duration = prv_get_config(inout_game)->frightened_duration_ms;

    if (duration == 0U)
    {
        /* Level 5: a power pellet is only points (§10.9). */
        return;
    }

    inout_game->frightened_remaining_ms = duration;

    /* Immediately, not at the next tick: the ghosts still have their own step to take later
     * in this same tick, and a ghost that meets Pacman during it must already be edible
     * (§10.5). Waiting would also publish a frame that says "frightened" with four ghosts
     * drawn as if they were not. */
    prv_apply_mode(inout_game);

    prv_publish(inout_game, MSG_GAME_FRIGHTENED_STARTED, NULL, 0U);
}

/* --- eating and collisions ----------------------------------------------- */

static void prv_eat_under_pacman(game_t* const inout_game)
{
    const cell_t cell = pacman_get_cell(&inout_game->pacman);
    const playfield_pellet_e eaten = playfield_eat_pellet(&inout_game->playfield, cell);
    msg_pellet_eaten_t payload = {0};

    if (eaten == PLAYFIELD_PELLET_NONE)
    {
        return;
    }

    payload.is_power_pellet = (eaten == PLAYFIELD_PELLET_POWER);

    prv_publish(inout_game, MSG_GAME_PELLET_EATEN, &payload, (uint16_t)sizeof(payload));

    if (payload.is_power_pellet)
    {
        prv_start_frightened(inout_game);
    }
}

static void prv_lose_life(game_t* const inout_game)
{
    ASSERT(inout_game->lives > 0U);

    --inout_game->lives;

    if (inout_game->lives == 0U)
    {
        inout_game->state = GAME_STATE_OVER;

        return;
    }

    /* Lives remain: everyone back to their starting cells, the maze as it stands (§10.7). */
    prv_place_entities(inout_game);
}

/* §10.7's two ways of meeting: sharing a cell, or swapping cells in the same tick and so
 * passing through each other. The second is easy to miss and shows up as a ghost that
 * walks straight through Pacman — which is why the previous cells are threaded in here. */
static bool prv_have_met(cell_t in_pacman_cell, cell_t in_pacman_previous_cell,
                         cell_t in_ghost_cell, cell_t in_ghost_previous_cell)
{
    if (playfield_are_cells_equal(in_pacman_cell, in_ghost_cell))
    {
        return true;
    }

    return playfield_are_cells_equal(in_pacman_cell, in_ghost_previous_cell)
           && playfield_are_cells_equal(in_ghost_cell, in_pacman_previous_cell);
}

/* Settle every meeting between Pacman and a ghost. Returns false once the run has ended, so
 * the caller stops advancing a finished game. */
static bool prv_resolve_meetings(game_t* const inout_game, cell_t in_pacman_previous_cell,
                                 const cell_t* const in_ghost_previous_cells)
{
    const cell_t pacman_cell = pacman_get_cell(&inout_game->pacman);

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_t* const ghost = &inout_game->ghosts[index];

        if (!prv_have_met(pacman_cell, in_pacman_previous_cell, ghost_get_cell(ghost),
                          in_ghost_previous_cells[index]))
        {
            continue;
        }

        if (ghost_is_frightened(ghost))
        {
            prv_publish(inout_game, MSG_GAME_GHOST_EATEN, NULL, 0U);
            ghost_send_to_pen(ghost, playfield_get_pen_cell(&inout_game->playfield,
                                                            (uint8_t)(index
                                                                      % PLAYFIELD_PEN_CELL_COUNT)));

            continue;
        }

        prv_lose_life(inout_game);

        return inout_game->state == GAME_STATE_RUNNING;
    }

    return true;
}

static bool prv_check_level_cleared(game_t* const inout_game)
{
    if (!playfield_is_cleared(&inout_game->playfield))
    {
        return true;
    }

    if (inout_game->level >= PLAYFIELD_LEVEL_COUNT)
    {
        /* The final maze: the run is won (FR-027). */
        inout_game->state = GAME_STATE_WON;

        return false;
    }

    /* Score and lives carry over (FR-021/§10.9). */
    prv_load_level(inout_game, (uint8_t)(inout_game->level + 1U));

    return true;
}

/* --- movement ------------------------------------------------------------ */

static bool prv_move_pacman(game_t* const inout_game)
{
    const cell_t previous_cell = pacman_get_cell(&inout_game->pacman);
    cell_t ghost_previous_cells[GHOST_COUNT];

    (void)pacman_advance(&inout_game->pacman, &inout_game->playfield);

    prv_eat_under_pacman(inout_game);

    /* The ghosts did not move in this step, so their "previous" cells are where they are —
     * a swap is impossible, only a shared cell. */
    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_previous_cells[index] = ghost_get_cell(&inout_game->ghosts[index]);
    }

    if (!prv_resolve_meetings(inout_game, previous_cell, ghost_previous_cells))
    {
        return false;
    }

    return prv_check_level_cleared(inout_game);
}

static bool prv_move_ghosts(game_t* const inout_game)
{
    const cell_t pacman_cell = pacman_get_cell(&inout_game->pacman);
    const direction_e pacman_direction = pacman_get_direction(&inout_game->pacman);
    const cell_t blinky_cell = ghost_get_cell(&inout_game->ghosts[GHOST_BLINKY]);
    cell_t ghost_previous_cells[GHOST_COUNT];

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_previous_cells[index] = ghost_get_cell(&inout_game->ghosts[index]);
    }

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        (void)ghost_advance(&inout_game->ghosts[index], &inout_game->playfield, pacman_cell,
                            pacman_direction, blinky_cell);
    }

    /* Pacman stood still during this step, so his previous cell is his current one. */
    return prv_resolve_meetings(inout_game, pacman_cell, ghost_previous_cells);
}

static uint32_t prv_get_ghost_period_ms(const game_t* const in_game)
{
    const uint32_t period = prv_get_config(in_game)->ghost_move_period_ms;

    if (in_game->frightened_remaining_ms > 0U)
    {
        return period * FRIGHTENED_SPEED_DIVISOR;
    }

    return period;
}

/* --- the frame ----------------------------------------------------------- */

static void prv_publish_snapshot(game_t* const inout_game)
{
    /* Fill the buffer the reader is *not* holding, then flip — so a frame is never handed
     * out half-written (R-007). */
    const uint8_t back_index = (uint8_t)(1U - inout_game->front_snapshot_index);
    game_snapshot_t* const snapshot = &inout_game->snapshots[back_index];

    ++inout_game->snapshot_version;

    snapshot->version = inout_game->snapshot_version;
    snapshot->level = inout_game->level;
    snapshot->lives = inout_game->lives;
    snapshot->score = score_get_total(&inout_game->score);
    snapshot->state = inout_game->state;
    snapshot->is_frightened_active = inout_game->frightened_remaining_ms > 0U;
    snapshot->pacman_cell = pacman_get_cell(&inout_game->pacman);
    snapshot->pacman_direction = pacman_get_direction(&inout_game->pacman);

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        snapshot->ghost_cells[index] = ghost_get_cell(&inout_game->ghosts[index]);
        snapshot->ghost_is_frightened[index] = ghost_is_frightened(&inout_game->ghosts[index]);
    }

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};

            snapshot->pellets[y][x] = playfield_get_pellet(&inout_game->playfield, cell);
        }
    }

    inout_game->front_snapshot_index = back_index;
}

/* ==========================================================================
 * game - public
 * ========================================================================= */

void game_init(game_t* inout_game)
{
    ASSERT(inout_game != NULL);

    prv_init_bus(inout_game);

    inout_game->state = GAME_STATE_IDLE;
    inout_game->lives = 0U;
    inout_game->front_snapshot_index = 0U;
    inout_game->snapshot_version = 0U;

    memset(inout_game->snapshots, 0, sizeof(inout_game->snapshots));

    prv_load_level(inout_game, PLAYFIELD_FIRST_LEVEL);
    prv_publish_snapshot(inout_game);
}

void game_start(game_t* inout_game)
{
    ASSERT(inout_game != NULL);

    /* A fresh bus rather than reset fields, so the score, the bonus chain and the
     * subscriptions are rebuilt together and cannot drift apart. */
    prv_init_bus(inout_game);

    inout_game->lives = GAME_STARTING_LIVES;
    inout_game->state = GAME_STATE_RUNNING;

    prv_load_level(inout_game, PLAYFIELD_FIRST_LEVEL);
    prv_publish_snapshot(inout_game);
}

void game_set_direction(game_t* inout_game, direction_e in_direction)
{
    ASSERT(inout_game != NULL);

    if (inout_game->state != GAME_STATE_RUNNING)
    {
        return;
    }

    pacman_set_intent(&inout_game->pacman, in_direction);
}

void game_tick(game_t* inout_game, uint32_t in_elapsed_ms)
{
    const uint32_t pacman_period = GAME_PACMAN_MOVE_PERIOD_MS;
    uint32_t ghost_period;

    ASSERT(inout_game != NULL);

    if (inout_game->state != GAME_STATE_RUNNING)
    {
        return;
    }

    prv_advance_timers(inout_game, in_elapsed_ms);
    prv_apply_mode(inout_game);

    inout_game->pacman_move_elapsed_ms += in_elapsed_ms;
    inout_game->ghost_move_elapsed_ms += in_elapsed_ms;

    while ((inout_game->pacman_move_elapsed_ms >= pacman_period)
           && (inout_game->state == GAME_STATE_RUNNING))
    {
        inout_game->pacman_move_elapsed_ms -= pacman_period;

        if (!prv_move_pacman(inout_game))
        {
            break;
        }
    }

    ghost_period = prv_get_ghost_period_ms(inout_game);

    while ((inout_game->ghost_move_elapsed_ms >= ghost_period)
           && (inout_game->state == GAME_STATE_RUNNING))
    {
        inout_game->ghost_move_elapsed_ms -= ghost_period;

        if (!prv_move_ghosts(inout_game))
        {
            break;
        }
    }

    prv_deliver_events(inout_game);
    prv_publish_snapshot(inout_game);
}

const game_snapshot_t* game_get_snapshot(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return &in_game->snapshots[in_game->front_snapshot_index];
}

game_state_e game_get_state(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->state;
}

uint32_t game_get_score(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return score_get_total(&in_game->score);
}

uint8_t game_get_lives(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->lives;
}

uint8_t game_get_level(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->level;
}

bool game_is_frightened_active(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->frightened_remaining_ms > 0U;
}
