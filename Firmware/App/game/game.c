#include "game.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "agent.h"
#include "custom_assert.h"
#include "difficulty.h"
#include "ghost.h"
#include "maze_gen.h"
#include "msg.h"
#include "msg_broker.h"
#include "pacman.h"
#include "playfield.h"
#include "rng_bsp.h"
#include "score.h"

/* ==========================================================================
 * game - private
 * ========================================================================= */

/* A whole step in \ref cell_progress_t units, and the largest value that still means
 * "not yet arrived" — 256 would be the next cell, which is the model's job to say. */
#define PROGRESS_FULL_STEP (256U)
#define PROGRESS_MAX       (255U)

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

    msg_broker_init(&inout_game->internal_broker, inout_game->internal_msg_buffer, GAME_INTERNAL_BROKER_CAPACITY);
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

/* --- the jitter (FR-044) ---------------------------------------------------
 *
 * Every timing the ghosts are paced by moves a little from run to run, so two runs of the same
 * level are not the same level: the scatter/chase plan, the frightened window, the idle timer that
 * pushes a ghost out when nobody is eating, and how many dots each ghost waits for.
 *
 * **Two bounds, and the second is the one that matters.** The owner asked for about two seconds. Two
 * seconds is also longer than several of the arcade's own phases — at level 5 the plan has scatter
 * phases of a twentieth of a second (§10.9) — so a flat two seconds would not vary those, it would
 * *replace* them, and the schedule's character is the thing worth keeping. So the jitter is also
 * capped at half the nominal value: a 20 s phase moves by 2 s, a 5 s phase by 2 s, a 1/20 s phase by
 * a millisecond nobody will notice. Nothing can reach zero, and nothing can double.
 *
 * A jitter is drawn when a timing *starts*, not when it is read — reading is per tick, and a
 * duration that changed under the timer counting it down would never expire. */
#define TIMING_JITTER_MS      (2000U)
#define TIMING_JITTER_DIVISOR (2U)

/* How many dots either way a ghost's wait may move. In dots rather than milliseconds because that
 * is the unit the house counts in; three is about a second and a half of eating. */
#define HOUSE_DOT_JITTER      (3U)

static uint32_t prv_jitter_ms(const game_t* const in_game, uint32_t in_nominal_ms)
{
    if (!in_game->config.has_timing_jitter || (in_nominal_ms == 0U))
    {
        return in_nominal_ms;
    }

    const uint32_t bound = (in_nominal_ms / TIMING_JITTER_DIVISOR) < TIMING_JITTER_MS
                               ? (in_nominal_ms / TIMING_JITTER_DIVISOR)
                               : TIMING_JITTER_MS;

    if (bound == 0U)
    {
        return in_nominal_ms;
    }

    /* `[nominal - bound, nominal + bound]`, so the mean is the nominal value and a level is neither
     * systematically easier nor harder than the table says. */
    return (in_nominal_ms - bound) + rng_bsp_get_below((2U * bound) + 1U);
}

/* What the table says a ghost waits for, in pellets eaten. Pinky's is always nothing, so he
 * leaves the moment a level begins; Blinky is never in there to ask. */
static uint16_t prv_get_nominal_dot_limit(const game_t* const in_game, uint8_t in_index)
{
    switch ((ghost_personality_e)in_index)
    {
        case GHOST_INKY: return in_game->difficulty.inky_dot_limit;
        case GHOST_CLYDE: return in_game->difficulty.clyde_dot_limit;
        default: return 0U;
    }
}

/* --- placement ----------------------------------------------------------- */

/* Put everyone back where the level starts, keeping score, lives and eaten pellets. Used
 * both after losing a life and when a new level loads (§10.7). */
static void prv_place_entities(game_t* const inout_game)
{
    pacman_reset(&inout_game->pacman, playfield_get_pacman_start(&inout_game->playfield));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        const cell_t start = playfield_get_ghost_start(&inout_game->playfield, index);

        /* Blinky's cell is outside the house and the other three are inside it (§10.2), so
         * the maze decides who has to come out rather than the rules restating it. */
        ghost_reset(&inout_game->ghosts[index], (ghost_personality_e)index, start,
                    playfield_is_house(&inout_game->playfield, start));
        inout_game->ghost_move_elapsed_ms[index] = 0U;
        inout_game->did_ghost_move[index] = false;
    }

    inout_game->pacman_move_elapsed_ms = 0U;
    inout_game->did_pacman_eat_last_step = false;
    inout_game->did_pacman_move = false;
    inout_game->dots_idle_ms = 0U;
    inout_game->frightened_remaining_ms = 0U;
    inout_game->phase_index = 0U;
    inout_game->phase_remaining_ms = prv_jitter_ms(inout_game, inout_game->difficulty.phase_durations_ms[0]);
    inout_game->house_idle_limit_ms = prv_jitter_ms(inout_game, inout_game->difficulty.house_idle_limit_ms);

    /* Drawn once per level rather than per ghost release, so a ghost's wait is a property of the
     * level a player is in and not something that changes while they are watching the house. */
    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        const uint16_t nominal = prv_get_nominal_dot_limit(inout_game, index);

        inout_game->ghost_dot_limit[index] = nominal;

        if (inout_game->config.has_timing_jitter && (nominal > 0U))
        {
            /* Never below one: a limit of zero means "leaves at once", which is Pinky's rule and
             * not something the jitter may hand to Inky or Clyde by accident. */
            const uint16_t span = (uint16_t)((2U * HOUSE_DOT_JITTER) + 1U);
            const int32_t moved = (int32_t)nominal - (int32_t)HOUSE_DOT_JITTER + (int32_t)rng_bsp_get_below(span);

            inout_game->ghost_dot_limit[index] = (uint16_t)((moved < 1) ? 1 : moved);
        }
    }
}

static uint16_t prv_get_dot_limit(const game_t* const in_game, uint8_t in_index);

/* The seed for one level's maze.
 *
 * Mixed rather than `seed + level`, so that consecutive levels are not neighbours in the
 * generator's own sequence: two xorshift32 streams started one apart diverge after a few
 * words, but their first words do not — and the first words are what place the first pieces,
 * which is the corner of the maze a player looks at first. */
static uint32_t prv_get_level_maze_seed(uint32_t in_run_seed, uint8_t in_level)
{
    uint32_t seed = in_run_seed + (0x9E3779B9U * (uint32_t)in_level);

    seed ^= seed >> 16;
    seed *= 0x7FEB352DU;
    seed ^= seed >> 15;

    return seed;
}

/* Turn every power pellet into an ordinary one, for a run configured without them.
 *
 * A substitution rather than a removal: the pellet counts stay as they were, so "the level is
 * cleared" keeps meaning what it meant and the level-clear path is the same code. Writes the
 * cells directly because that is what a `playfield_t` is — the type publishes its grid, and a
 * demotion is not an event anything needs to hear about. */
static void prv_demote_power_pellets(game_t* const inout_game)
{
    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        for (uint8_t column = 0U; column < PLAYFIELD_WIDTH; ++column)
        {
            if (inout_game->playfield.pellets[row][column] == PLAYFIELD_PELLET_POWER)
            {
                inout_game->playfield.pellets[row][column] = PLAYFIELD_PELLET_NORMAL;
            }
        }
    }
}

static void prv_load_level(game_t* const inout_game, uint8_t in_level)
{
    inout_game->level = in_level;

    /* Before the entities are placed: placing them arms the first scatter phase, and how
     * long that lasts is the level's business (§10.9). */
    difficulty_get(in_level, &inout_game->difficulty);

    if (!inout_game->is_maze_fixed)
    {
        maze_gen_generate(&inout_game->maze, prv_get_level_maze_seed(inout_game->maze_seed, in_level));
    }

    playfield_load_from_map(&inout_game->playfield, &inout_game->maze);

    if (!inout_game->config.has_power_pellets)
    {
        prv_demote_power_pellets(inout_game);
    }

    prv_place_entities(inout_game);

    /* A new level starts the personal counters over and puts the global one away (§10.4).
     * Losing a life is the other way round, and #prv_lose_life does that. */
    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        inout_game->ghost_dot_counter[index] = 0U;
    }

    inout_game->global_dot_counter = 0U;
    inout_game->is_global_dot_counter_active = false;

    /* A limit of zero is already met, so those ghosts walk out as the level begins rather
     * than waiting for a first pellet. Pinky's limit is always zero, and by level three so
     * are Inky's and Clyde's — which is why the later levels are busy immediately. */
    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        if (ghost_is_waiting_in_house(&inout_game->ghosts[index]) && (prv_get_dot_limit(inout_game, index) == 0U))
        {
            ghost_release_from_house(&inout_game->ghosts[index]);
        }
    }
}

/* What this level's ghost actually waits for: the table's figure, moved by the jitter drawn when the
 * level loaded (FR-044). */
static uint16_t prv_get_dot_limit(const game_t* const in_game, uint8_t in_index)
{
    return in_game->ghost_dot_limit[in_index];
}

/* Which ghost's counter is running: the most-preferred one still shut in, in the order
 * Pinky, Inky, Clyde. Returns #GHOST_COUNT when the house is empty of waiters. */
static uint8_t prv_get_waiting_ghost(const game_t* const in_game)
{
    static const ghost_personality_e k_preference[] = {GHOST_PINKY, GHOST_INKY, GHOST_CLYDE};

    for (uint8_t index = 0U; index < (sizeof(k_preference) / sizeof(k_preference[0])); ++index)
    {
        const uint8_t candidate = (uint8_t)k_preference[index];

        if (ghost_is_waiting_in_house(&in_game->ghosts[candidate]))
        {
            return candidate;
        }
    }

    return GHOST_COUNT;
}

/* The global counter's release points (§10.4). Clyde's is also where the scheme hands back
 * to the personal counters, which is the one way it can be switched off. */
#define GLOBAL_RELEASE_PINKY (7U)
#define GLOBAL_RELEASE_INKY  (17U)
#define GLOBAL_RELEASE_CLYDE (32U)

static void prv_release_from_house(game_t* const inout_game, uint8_t in_index)
{
    ghost_release_from_house(&inout_game->ghosts[in_index]);
}

/* One pellet's worth of progress towards letting the next ghost out. */
static void prv_count_dot_for_the_house(game_t* const inout_game)
{
    const uint8_t waiting = prv_get_waiting_ghost(inout_game);

    inout_game->dots_idle_ms = 0U;

    if (waiting >= GHOST_COUNT)
    {
        return;
    }

    if (inout_game->is_global_dot_counter_active)
    {
        ++inout_game->global_dot_counter;

        if ((inout_game->global_dot_counter >= GLOBAL_RELEASE_CLYDE)
            && ghost_is_waiting_in_house(&inout_game->ghosts[GHOST_CLYDE]))
        {
            /* Clyde still indoors at 32 is the only thing that retires the global counter
             * and hands back to the personal ones. */
            prv_release_from_house(inout_game, (uint8_t)GHOST_CLYDE);
            inout_game->is_global_dot_counter_active = false;
            inout_game->global_dot_counter = 0U;
        }
        else if ((inout_game->global_dot_counter >= GLOBAL_RELEASE_INKY) && (waiting == (uint8_t)GHOST_INKY))
        {
            prv_release_from_house(inout_game, waiting);
        }
        else if ((inout_game->global_dot_counter >= GLOBAL_RELEASE_PINKY) && (waiting == (uint8_t)GHOST_PINKY))
        {
            prv_release_from_house(inout_game, waiting);
        }
        else
        {
            /* Not there yet. */
        }

        return;
    }

    ++inout_game->ghost_dot_counter[waiting];

    if (inout_game->ghost_dot_counter[waiting] >= prv_get_dot_limit(inout_game, waiting))
    {
        prv_release_from_house(inout_game, waiting);
    }
}

/* Nobody is eating. Push the next one out anyway, or three of them stay in for ever. */
static void prv_advance_house_idle_timer(game_t* const inout_game, uint32_t in_elapsed_ms)
{
    const uint8_t waiting = prv_get_waiting_ghost(inout_game);

    if (waiting >= GHOST_COUNT)
    {
        return;
    }

    inout_game->dots_idle_ms += in_elapsed_ms;

    if (inout_game->dots_idle_ms >= inout_game->house_idle_limit_ms)
    {
        prv_release_from_house(inout_game, waiting);
        inout_game->dots_idle_ms = 0U;
    }
}

/* --- scatter / chase / frightened ---------------------------------------- */

/* Which non-frightened mode the plan of §10.4/§10.9 calls for right now. The plan
 * alternates from scatter, so even entries are scatter and odd ones chase; once it is
 * used up it is chase for the rest of the level. */
static ghost_mode_e prv_get_scheduled_mode(const game_t* const in_game)
{
    if (in_game->phase_index >= in_game->difficulty.phase_count)
    {
        return GHOST_MODE_CHASE;
    }

    return ((in_game->phase_index % 2U) == 0U) ? GHOST_MODE_SCATTER : GHOST_MODE_CHASE;
}

/* Which Cruise Elroy stage Blinky is in: 0 for none, 1 or 2 as the maze empties (§10.9).
 *
 * Stage 2's threshold is the lower of the two, so it is tested first — at 10 pellets left
 * on level 1 both conditions hold and only the faster one is meant. */
static uint8_t prv_get_elroy_stage(const game_t* const in_game)
{
    const uint16_t remaining = playfield_get_remaining_pellet_count(&in_game->playfield);

    if (remaining <= in_game->difficulty.elroy2_pellets_left)
    {
        return 2U;
    }

    if (remaining <= in_game->difficulty.elroy1_pellets_left)
    {
        return 1U;
    }

    return 0U;
}

static uint32_t prv_get_phase_duration(const game_t* const in_game)
{
    if (in_game->phase_index >= in_game->difficulty.phase_count)
    {
        return 0U;
    }

    return in_game->difficulty.phase_durations_ms[in_game->phase_index];
}

/* Push the mode every entity should be in. Safe to call every tick: a mode change moves the
 * ghost's destination and never its facing (§10.1), so nothing here can turn one around. */
static void prv_apply_mode(game_t* const inout_game)
{
    const bool is_frightened = inout_game->frightened_remaining_ms > 0U;
    const ghost_mode_e scheduled = prv_get_scheduled_mode(inout_game);
    const bool is_elroy_awake = prv_get_elroy_stage(inout_game) > 0U;

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_mode_e mode = is_frightened ? GHOST_MODE_FRIGHTENED : scheduled;

        /* Cruise Elroy does not go home. Once the maze is empty enough to wake him,
         * Blinky keeps hunting through the scatter phases the others take off — which is
         * what turns the end of a level from a breather into the hard part. */
        if ((index == (uint8_t)GHOST_BLINKY) && is_elroy_awake && (mode == GHOST_MODE_SCATTER))
        {
            mode = GHOST_MODE_CHASE;
        }

        ghost_set_mode(&inout_game->ghosts[index], mode);
    }
}

static void prv_advance_timers(game_t* const inout_game, uint32_t in_elapsed_ms)
{
    if (inout_game->frightened_remaining_ms > 0U)
    {
        /* The frightened window freezes the scatter/chase plan rather than running it down
         * in the background, so the ghosts resume the phase they were interrupted in. */
        inout_game->frightened_remaining_ms = (inout_game->frightened_remaining_ms > in_elapsed_ms)
                                                  ? (inout_game->frightened_remaining_ms - in_elapsed_ms)
                                                  : 0U;

        return;
    }

    if (inout_game->phase_index >= inout_game->difficulty.phase_count)
    {
        /* The plan is used up and it is chase for the rest of the level. Without this the
         * index would keep climbing every tick against a duration of zero. */
        return;
    }

    if (inout_game->phase_remaining_ms > in_elapsed_ms)
    {
        inout_game->phase_remaining_ms -= in_elapsed_ms;

        return;
    }

    ++inout_game->phase_index;
    inout_game->phase_remaining_ms = prv_jitter_ms(inout_game, prv_get_phase_duration(inout_game));
}

static void prv_start_frightened(game_t* const inout_game)
{
    const uint32_t duration = prv_jitter_ms(inout_game, inout_game->difficulty.frightened_duration_ms);

    if (duration == 0U)
    {
        /* Level 17, and every level from 19 on: a power pellet is only points (§10.9). */
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

    /* Read by the *next* step, which is slower for it (§10.9). Set either way: a step onto
     * an already-cleared cell is what makes him quick again. */
    inout_game->did_pacman_eat_last_step = (eaten != PLAYFIELD_PELLET_NONE);

    if (eaten != PLAYFIELD_PELLET_NONE)
    {
        prv_count_dot_for_the_house(inout_game);
    }

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

    /* Lives remain: everyone back to their starting cells, the maze as it stands (§10.7).
     * The personal dot counters are set aside — not reset — for the global one, so a fresh
     * start after a death is paced the same however late in the level it happened. */
    prv_place_entities(inout_game);

    inout_game->is_global_dot_counter_active = true;
    inout_game->global_dot_counter = 0U;
}

/* §10.7's two ways of meeting: sharing a cell, or swapping cells in the same tick and so
 * passing through each other. The second is easy to miss and shows up as a ghost that
 * walks straight through Pacman — which is why the previous cells are threaded in here. */
static bool prv_have_met(cell_t in_pacman_cell, cell_t in_pacman_previous_cell, cell_t in_ghost_cell,
                         cell_t in_ghost_previous_cell)
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
    /* One guard for both callers — Pacman's step and a ghost's — so a run without ghosts
     * cannot lose a life down a path this function does not own. */
    if (!inout_game->config.has_ghosts)
    {
        return true;
    }

    const cell_t pacman_cell = pacman_get_cell(&inout_game->pacman);

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_t* const ghost = &inout_game->ghosts[index];

        if (!prv_have_met(pacman_cell, in_pacman_previous_cell, ghost_get_cell(ghost), in_ghost_previous_cells[index]))
        {
            continue;
        }

        if (ghost_is_frightened(ghost))
        {
            ++inout_game->ghosts_eaten;

            prv_publish(inout_game, MSG_GAME_GHOST_EATEN, NULL, 0U);
            /* Revived in the middle of the house, which is Pinky's starting cell, and it
             * has to walk out through the gate like the others. */
            ghost_send_to_pen(ghost, playfield_get_ghost_start(&inout_game->playfield, (uint8_t)GHOST_PINKY));

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

    if (difficulty_is_final_level(inout_game->level))
    {
        /* The whole difficulty curve has been walked: the run is won (FR-027). */
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

    inout_game->did_pacman_move = pacman_advance(&inout_game->pacman, &inout_game->playfield);

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

/* One ghost takes one step. They used to move as a block, and could, because they shared a
 * speed; Cruise Elroy and the tunnel ended that (§10.9), so each one is stepped on its own
 * clock and its meeting with Pacman is settled straight away. */
static bool prv_move_ghost(game_t* const inout_game, uint8_t in_index)
{
    const cell_t pacman_cell = pacman_get_cell(&inout_game->pacman);
    const direction_e pacman_direction = pacman_get_direction(&inout_game->pacman);
    const cell_t blinky_cell = ghost_get_cell(&inout_game->ghosts[GHOST_BLINKY]);
    cell_t ghost_previous_cells[GHOST_COUNT];

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        ghost_previous_cells[index] = ghost_get_cell(&inout_game->ghosts[index]);
    }

    inout_game->did_ghost_move[in_index] = ghost_advance(&inout_game->ghosts[in_index], &inout_game->playfield,
                                                         pacman_cell, pacman_direction, blinky_cell);

    /* The gate shuts behind it. Checked here rather than in the ghost because where the
     * house is is the maze's knowledge (§10.4). */
    if (ghost_is_in_house(&inout_game->ghosts[in_index])
        && !playfield_is_house(&inout_game->playfield, ghost_get_cell(&inout_game->ghosts[in_index])))
    {
        ghost_note_left_house(&inout_game->ghosts[in_index]);
    }

    /* Pacman stood still during this step, so his previous cell is his current one. */
    return prv_resolve_meetings(inout_game, pacman_cell, ghost_previous_cells);
}

/* How long Pacman takes over one cell right now (§10.9).
 *
 * Three things move it: the level, whether the ghosts are frightened — he is bolder and
 * quicker while they are — and whether he is chewing. The last is why a corridor you have
 * already cleared is faster than a full one, and it is the whole reason a good player
 * clears an escape route before going for a power pellet. */
static uint32_t prv_get_pacman_period_ms(const game_t* const in_game)
{
    const difficulty_t* const difficulty = &in_game->difficulty;
    const bool is_eating = in_game->did_pacman_eat_last_step;

    if (in_game->frightened_remaining_ms > 0U)
    {
        return is_eating ? difficulty->pacman_frightened_eating_period_ms : difficulty->pacman_frightened_period_ms;
    }

    return is_eating ? difficulty->pacman_eating_period_ms : difficulty->pacman_period_ms;
}

/* How long one ghost takes over one cell right now (§10.9). No two of them need agree.
 *
 * The tunnel wins over everything else, frightened included: it is the arcade's hard cap
 * on how fast anything crosses that stretch, and it is what makes the tunnel the one
 * place a cornered Pacman can reliably break away. */
static uint32_t prv_get_ghost_period_ms(const game_t* const in_game, uint8_t in_index)
{
    const ghost_t* const ghost = &in_game->ghosts[in_index];
    const difficulty_t* const difficulty = &in_game->difficulty;

    if (playfield_is_tunnel(&in_game->playfield, ghost_get_cell(ghost)))
    {
        return difficulty->ghost_tunnel_period_ms;
    }

    if (ghost_is_frightened(ghost))
    {
        return difficulty->ghost_frightened_period_ms;
    }

    if (in_index == (uint8_t)GHOST_BLINKY)
    {
        const uint8_t stage = prv_get_elroy_stage(in_game);

        if (stage == 2U)
        {
            return difficulty->elroy2_period_ms;
        }

        if (stage == 1U)
        {
            return difficulty->elroy1_period_ms;
        }
    }

    return difficulty->ghost_period_ms;
}

/* --- the state the view sees --------------------------------------------- */

/* How far an entity has come into its cell from the one before it, in 1/256ths.
 *
 * This measures the step **already taken**, which is what makes it exact: the cell it came
 * from is a fact, whereas the cell it will go to next is not decided until the next step.
 * The version before this one interpolated forward along the current facing, and that is
 * wrong precisely at a corner — see \ref cell_progress_t.
 *
 * An entity that did not move reports "arrived": one stopped against a wall keeps its
 * facing (§10.1) and its timer keeps running, and without this it would be drawn sliding
 * in from the cell behind it once per period, on the spot.
 */
static cell_progress_t prv_get_progress(bool in_did_move, direction_e in_direction, uint32_t in_elapsed_ms,
                                        uint32_t in_period_ms)
{
    uint32_t progress;

    if (!in_did_move || (in_period_ms == 0U) || (in_direction == DIRECTION_NONE))
    {
        return MSG_CELL_PROGRESS_ARRIVED;
    }

    progress = (in_elapsed_ms * PROGRESS_FULL_STEP) / in_period_ms;

    return (cell_progress_t)((progress > PROGRESS_MAX) ? PROGRESS_MAX : progress);
}

static msg_actor_t prv_describe_actor(cell_t in_cell, direction_e in_direction, bool in_did_move,
                                      uint32_t in_elapsed_ms, uint32_t in_period_ms)
{
    msg_actor_t actor;

    actor.column = (uint8_t)in_cell.x;
    actor.row = (uint8_t)in_cell.y;
    actor.direction = (uint8_t)in_direction;
    actor.progress = prv_get_progress(in_did_move, in_direction, in_elapsed_ms, in_period_ms);

    return actor;
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

    /* An idle game still holds a whole level, so everything downstream — the state message,
     * the view, the panel — has something consistent to work with before a run begins. The
     * seed is fixed because nothing is being played yet; #game_start replaces it. */
    inout_game->maze_seed = 1U;
    inout_game->is_maze_fixed = false;

    /* Before the level loads, because loading one reads the rules. */
    game_get_default_config(&inout_game->config);

    prv_load_level(inout_game, DIFFICULTY_FIRST_LEVEL);
}

/* What both ways of starting a run have in common. */
static void prv_begin_run(game_t* const inout_game)
{
    /* A fresh bus rather than reset fields, so the score, the bonus chain and the
     * subscriptions are rebuilt together and cannot drift apart. */
    prv_init_bus(inout_game);

    inout_game->lives = GAME_STARTING_LIVES;
    inout_game->state = GAME_STATE_RUNNING;
    inout_game->ghosts_eaten = 0U;

    prv_load_level(inout_game, DIFFICULTY_FIRST_LEVEL);
}

void game_get_default_config(game_config_t* out_config)
{
    ASSERT(out_config != NULL);

    out_config->has_ghosts = true;
    out_config->has_timing_jitter = true;
    out_config->has_power_pellets = true;
}

void game_start(game_t* inout_game, uint32_t in_maze_seed)
{
    ASSERT(inout_game != NULL);

    game_config_t config;
    game_get_default_config(&config);

    game_start_configured(inout_game, in_maze_seed, &config);
}

void game_start_configured(game_t* inout_game, uint32_t in_maze_seed, const game_config_t* in_config)
{
    ASSERT(inout_game != NULL);
    ASSERT(in_config != NULL);

    inout_game->config = *in_config;
    inout_game->maze_seed = in_maze_seed;
    inout_game->is_maze_fixed = false;

    prv_begin_run(inout_game);
}

void game_start_on_map(game_t* inout_game, const playfield_map_t* in_map)
{
    ASSERT(inout_game != NULL);

    game_config_t config;
    game_get_default_config(&config);

    game_start_on_map_configured(inout_game, in_map, &config);
}

void game_start_on_map_configured(game_t* inout_game, const playfield_map_t* in_map, const game_config_t* in_config)
{
    ASSERT(inout_game != NULL);
    ASSERT(in_map != NULL);
    ASSERT(in_config != NULL);

    inout_game->config = *in_config;
    inout_game->maze = *in_map;
    inout_game->is_maze_fixed = true;

    prv_begin_run(inout_game);
}

void game_start_on_normal_maze(game_t* inout_game)
{
    ASSERT(inout_game != NULL);

    /* Straight into the game's own copy, rather than through a `playfield_map_t` the caller holds:
     * one is 899 bytes, which is most of the kilobyte of stack the linker script reserves, and a
     * caller on the target would have to find that space somewhere for a value it hands straight
     * over. This is why "the normal maze" is a name in this API and not an argument to it. */
    playfield_get_arcade_map(&inout_game->maze);
    inout_game->is_maze_fixed = true;

    game_get_default_config(&inout_game->config);

    prv_begin_run(inout_game);
}

const playfield_map_t* game_get_maze(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return &in_game->maze;
}

const playfield_t* game_get_playfield(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return &in_game->playfield;
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
    ASSERT(inout_game != NULL);

    if (inout_game->state != GAME_STATE_RUNNING)
    {
        return;
    }

    prv_advance_timers(inout_game, in_elapsed_ms);
    prv_advance_house_idle_timer(inout_game, in_elapsed_ms);
    prv_apply_mode(inout_game);

    inout_game->pacman_move_elapsed_ms += in_elapsed_ms;

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        inout_game->ghost_move_elapsed_ms[index] += in_elapsed_ms;
    }

    /* The period is read again after every step, because a step is what changes it: eating
     * a pellet slows the next one down, and clearing the corridor speeds it back up. A
     * period of zero would spin here forever, so it ends the loop rather than being
     * asserted away under NDEBUG. */
    for (uint32_t period = prv_get_pacman_period_ms(inout_game);
         (period > 0U) && (inout_game->pacman_move_elapsed_ms >= period) && (inout_game->state == GAME_STATE_RUNNING);
         period = prv_get_pacman_period_ms(inout_game))
    {
        inout_game->pacman_move_elapsed_ms -= period;

        if (!prv_move_pacman(inout_game))
        {
            break;
        }
    }

    for (uint8_t index = 0U;
         inout_game->config.has_ghosts && (index < GHOST_COUNT) && (inout_game->state == GAME_STATE_RUNNING); ++index)
    {
        /* Likewise per ghost: a step can carry it into or out of the tunnel, and Blinky's
         * own step can be the one that empties the maze far enough to wake Elroy. */
        for (uint32_t period = prv_get_ghost_period_ms(inout_game, index);
             (period > 0U) && (inout_game->ghost_move_elapsed_ms[index] >= period)
             && (inout_game->state == GAME_STATE_RUNNING);
             period = prv_get_ghost_period_ms(inout_game, index))
        {
            inout_game->ghost_move_elapsed_ms[index] -= period;

            if (!prv_move_ghost(inout_game, index))
            {
                break;
            }
        }
    }

    prv_deliver_events(inout_game);
}

/* Whether the frightened ghosts should be drawn in their warning colour this instant.
 *
 * The window closes after `frightened_flash_count` flashes, each a dark half and a light
 * half, so the warning starts that far from the end and then alternates. Deciding it here
 * rather than in the view keeps "flashing" a fact about the game that a test can assert,
 * instead of an animation the view invents. */
static bool prv_are_frightened_ghosts_flashing(const game_t* const in_game)
{
    const uint32_t half_period = GAME_FRIGHTENED_FLASH_HALF_PERIOD_MS;
    const uint32_t warning_ms = (uint32_t)in_game->difficulty.frightened_flash_count * 2U * half_period;

    if ((in_game->frightened_remaining_ms == 0U) || (in_game->frightened_remaining_ms > warning_ms))
    {
        return false;
    }

    return ((in_game->frightened_remaining_ms / half_period) % 2U) == 0U;
}

void game_get_state_message(const game_t* in_game, msg_game_state_t* out_state)
{
    ASSERT(in_game != NULL);
    ASSERT(out_state != NULL);

    memset(out_state, 0, sizeof(*out_state));

    out_state->pacman = prv_describe_actor(pacman_get_cell(&in_game->pacman), pacman_get_direction(&in_game->pacman),
                                           in_game->did_pacman_move, in_game->pacman_move_elapsed_ms,
                                           prv_get_pacman_period_ms(in_game));

    for (uint8_t index = 0U; index < GHOST_COUNT; ++index)
    {
        const ghost_t* const ghost = &in_game->ghosts[index];

        out_state->ghosts[index] =
            prv_describe_actor(ghost_get_cell(ghost), ghost_get_direction(ghost), in_game->did_ghost_move[index],
                               in_game->ghost_move_elapsed_ms[index], prv_get_ghost_period_ms(in_game, index));

        if (ghost_is_frightened(ghost))
        {
            out_state->frightened_ghosts |= (uint8_t)(1U << index);
        }
    }

    out_state->are_frightened_ghosts_flashing = prv_are_frightened_ghosts_flashing(in_game);

    for (int16_t y = 0; y < PLAYFIELD_HEIGHT; ++y)
    {
        for (int16_t x = 0; x < PLAYFIELD_WIDTH; ++x)
        {
            const cell_t cell = {x, y};
            const playfield_pellet_e pellet = playfield_get_pellet(&in_game->playfield, cell);

            msg_cell_bitmap_set(out_state->has_pellet, (uint8_t)x, (uint8_t)y, pellet != PLAYFIELD_PELLET_NONE);
            msg_cell_bitmap_set(out_state->is_power, (uint8_t)x, (uint8_t)y, pellet == PLAYFIELD_PELLET_POWER);
        }
    }

    out_state->score = score_get_total(&in_game->score);
    out_state->lives = in_game->lives;
    out_state->level = in_game->level;
}

game_state_e game_get_state(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->state;
}

cell_t game_get_pacman_cell(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return pacman_get_cell(&in_game->pacman);
}

uint16_t game_get_ghosts_eaten(const game_t* in_game)
{
    ASSERT(in_game != NULL);

    return in_game->ghosts_eaten;
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
