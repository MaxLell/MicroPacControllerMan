#include "pacman_lookahead.h"

#include <string.h>

#include "custom_assert.h"
#include "pacman.h"

/* ==========================================================================
 * pacman_lookahead - private
 * ========================================================================= */

/*! \brief How long a simulated tick is.
 *
 * The frame period the game is really ticked at, and it is that on purpose: a coarser step would
 * simulate a game nobody plays, and the actor speeds are per-millisecond figures that a step of
 * the wrong size rounds differently. `game_session` ticks at this, so the search does.
 */
#define PACMAN_LOOKAHEAD_STEP_MS        (16U)

/*! \brief Ticks a single simulated cell may take before the walk gives up on it.
 *
 * A Pacman stopped against a wall never reaches another cell, and without a ceiling the loop
 * looking for the next one would run for ever. Seven ticks is a cell at level 1, so this is four
 * times what the slowest crawl needs and is a backstop rather than a working limit.
 */
#define PACMAN_LOOKAHEAD_MAX_CELL_TICKS (32U)

/*! \brief Cells a single junction-to-junction leg may run for.
 *
 * The arcade's longest corridor is shorter than this. It exists for the case a leg has no
 * junction to end at — a ring of corridor with no branch — where the walk would otherwise spend
 * the whole budget on one branch.
 */
#define PACMAN_LOOKAHEAD_MAX_LEG_CELLS  (24U)

/*! \brief What a point of score is worth against the other terms.
 *
 * Ten, so that the smallest thing worth having — one pellet, ten points — outranks the whole
 * range of the safety term below. Score is what the game is scored on; safety only sorts branches
 * that are worth the same.
 */
#define PACMAN_LOOKAHEAD_POINT_WEIGHT   (10)

/*! \brief What losing a life costs the branch that leads to it.
 *
 * Large enough that no reachable amount of score buys one. A search whose branches all die still
 * picks the one that dies latest, because the score gathered on the way is what separates them.
 */
#define PACMAN_LOOKAHEAD_DEATH_PENALTY  (100000)

/*! \brief Where the safety term stops caring, as a squared distance.
 *
 * 64, which is eight cells — the arcade's own figure for "far enough away", the radius Clyde
 * turns shy at (§10.4). Beyond it one ghost is as harmless as another and the branch should be
 * decided by what there is to eat.
 */
#define PACMAN_LOOKAHEAD_SAFE_DISTANCE  (64)

/*! \brief The order branches are tried in.
 *
 * Fixed, and part of the behaviour rather than an implementation detail, for two reasons. A
 * search that spends its cell budget stops where it is, so the order decides what a truncated
 * search got to look at. And two branches can be worth exactly the same, in which case the first
 * one tried wins — the same reason `pacman_ai` writes its search order down.
 */
static const direction_e g_branch_order[] = {DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_WEST};

/*! \brief One game per level of depth, because a branch has to be tried without losing the
 *         position it branched from.
 *
 * File-scope rather than on the stack: a `game_t` is about 15 kB and the target reserves a
 * kilobyte of stack (NFR-008). This is the memory that makes #PACMAN_LOOKAHEAD_MAX_DEPTH a
 * ceiling. */
static game_t g_clone[PACMAN_LOOKAHEAD_MAX_DEPTH];

/*! \brief The position the whole search is rooted at, taken once when the search begins.
 *
 * **A search that spans frames cannot root itself in the live game**, because the live game ticks
 * between its slices: branch one would be tried from a board the others never saw, and the values
 * being compared would be values of different games. So the root is copied, and every branch,
 * every slice and every deepening is measured against that one copy.
 *
 * It costs nothing in staleness. A decision has always been rooted at the moment Pacman entered
 * the cell and always taken effect at the moment he leaves it — `pacman_set_intent` queues, and he
 * adopts the queue when his step falls due — so thinking for longer changes how *deep* the answer
 * is and not how old the board it was worked out on is.
 *
 * It lives in **SRAM4**, the 16 kB the part has beside its main 256 and which nothing else uses. A
 * `game_t` is 15 kB, which is what SRAM4 has been "exactly big enough for" in three design
 * documents; this is the thing it was being kept for. On the host it is an ordinary object,
 * because a host has no SRAM4 and no reason for one. */
#if defined(PACMAN_HOST_BUILD) || defined(TEST)
static game_t g_root;
#else  /* defined(PACMAN_HOST_BUILD) || defined(TEST) */
__attribute__((section(".sram4"), used, aligned(4))) static game_t g_root;
#endif /* !defined(PACMAN_HOST_BUILD) && !defined(TEST) */

/*! \brief What is left of the search's budget, and what it has spent. Reset by every search. */
static uint16_t g_ticks_left;
static pacman_lookahead_report_t g_report;

/*! \brief Whether *this* deepening ran out of budget, as opposed to the search as a whole.
 *
 * Separate from the report's flag because the two answer different questions: the report says the
 * caller did not get everything it asked for, this says the iteration in progress is not worth
 * believing and its answer has to be thrown away. */
static bool g_is_iteration_truncated;

/*! \brief One level of the search, held where a recursive call used to hold it.
 *
 * The search is a tree walk and was written as one, which is the clearest way to say it and the
 * one thing it may not be: a C call stack cannot be put down half-way through and picked up in the
 * next frame. Everything a recursive `prv_value_of` kept in its own frame is kept here instead, so
 * the walk can stop between any two branches and resume where it left off
 * ([M6 §15.6](../../../Docu/Design/M6-Pacman-AI.md)).
 *
 * The *position* each level stands on is not in here: level 0 stands on #g_root and level `n` on
 * `g_clone[n - 1]`, which is the same arrangement the recursion had and is why the clones did not
 * have to grow. */
typedef struct
{
    uint8_t next_branch;           /*!< Index into #g_branch_order still to be tried      */
    int32_t best_value;            /*!< Best a tried branch has been worth                */
    direction_e best_direction;    /*!< Which branch that was                             */
    direction_e pending_direction; /*!< The branch whose subtree is being walked now      */
    bool has_branch;               /*!< Whether any branch was walkable at all            */

    /*! \brief Whether this level has been looked at once already.
     *
     * The recursion asked "is this a leaf, is the budget gone, is the run over?" **on the way in**,
     * once per call. A loop comes back to a level every time a child of it finishes, so without
     * this the question gets asked again on the way *out* — and a level that had spent the last of
     * the budget on its own children would then be recorded as a truncation and its perfectly
     * finished answer thrown away. Measured: it cost the depth-3 deepening on the decisions where
     * the budget ran out exactly as the search completed. */
    bool is_entered;
} pacman_lookahead_level_t;

/*! \brief One more than the depth, because the level *below* the deepest is where a leaf is
 *         priced. */
static pacman_lookahead_level_t g_level[PACMAN_LOOKAHEAD_MAX_DEPTH + 1U];
static uint8_t g_top;

/*! \brief The deepening in progress, the one to stop at, and whether one is part-way through. */
static uint8_t g_target_depth;
static uint8_t g_depth_ceiling;
static bool g_is_deepening_active;

/*! \brief Nothing further will change the answer — the ceiling was reached, the budget ran out, or
 *         the run is not running. */
static bool g_is_search_finished;

/*! \brief The deepest *complete* deepening's answer, which is what a caller is given. */
static direction_e g_best;

/* The straight-line distance to the nearest ghost that could kill him, squared and capped.
 *
 * Straight-line and not a maze distance on purpose: a maze distance is a breadth-first walk over
 * 868 cells, which is more than the whole leg that produced this position cost to simulate. This
 * term only sorts branches that scored the same, and for that a rough answer is the right price.
 * A frightened ghost is not counted — it is food, and `game` says which is which. */
static int32_t prv_safety_of(const game_t* const in_game)
{
    const cell_t pacman = game_get_pacman_cell(in_game);
    int32_t nearest = PACMAN_LOOKAHEAD_SAFE_DISTANCE;
    uint8_t index;

    for (index = 0U; index < GHOST_COUNT; ++index)
    {
        if (game_is_ghost_frightened(in_game, index))
        {
            continue;
        }

        const uint32_t squared = playfield_get_squared_distance(pacman, game_get_ghost_cell(in_game, index));

        if ((int32_t)squared < nearest)
        {
            nearest = (int32_t)squared;
        }
    }

    return nearest;
}

/* What a simulated position is worth, measured against the position the search started from.
 *
 * Everything here is a difference rather than an absolute, which is what makes the numbers mean
 * "what this branch got me" instead of "how the run is going". */
static int32_t prv_evaluate(const game_t* const in_leaf, const game_t* const in_root)
{
    const int32_t points = (int32_t)game_get_score(in_leaf) - (int32_t)game_get_score(in_root);
    const int32_t lives_lost = (int32_t)game_get_lives(in_root) - (int32_t)game_get_lives(in_leaf);

    return (points * PACMAN_LOOKAHEAD_POINT_WEIGHT) - (lives_lost * PACMAN_LOOKAHEAD_DEATH_PENALTY)
           + prv_safety_of(in_leaf);
}

/* Whether a cell is somewhere a decision gets made: more than the way in and one way on.
 *
 * Counted over all four neighbours including the one behind, so a dead end counts as a junction
 * too — turning round there is a choice, and the only one. */
static bool prv_is_junction(const game_t* const in_game)
{
    const playfield_t* const playfield = game_get_playfield(in_game);
    uint8_t open_count = 0U;
    uint8_t index;

    for (index = 0U; index < (uint8_t)(sizeof(g_branch_order) / sizeof(g_branch_order[0])); ++index)
    {
        if (pacman_may_step(&in_game->pacman, playfield, g_branch_order[index]))
        {
            ++open_count;
        }
    }

    return open_count > 2U;
}

/* Walk a clone from where it stands to the next place a decision is due, and say how far that
 * was in cells.
 *
 * The direction is set once and then the game steers: down a corridor there is nothing to decide,
 * so the walk ends at the next junction, at the end of the run, at the moment a life is lost, or
 * at a cap — whichever comes first. Cells rather than ticks are counted because a cell is what
 * the budget is denominated in and what the board was measured in. */
static uint16_t prv_walk_to_next_decision(game_t* const inout_game, direction_e in_direction)
{
    const uint8_t lives_at_start = game_get_lives(inout_game);
    cell_t previous = game_get_pacman_cell(inout_game);
    uint16_t cells = 0U;
    uint32_t ticks_on_this_cell = 0U;

    game_set_direction(inout_game, in_direction);

    while ((cells < PACMAN_LOOKAHEAD_MAX_LEG_CELLS) && (g_ticks_left > 0U)
           && (game_get_state(inout_game) == GAME_STATE_RUNNING))
    {
        game_tick(inout_game, PACMAN_LOOKAHEAD_STEP_MS);
        --g_ticks_left;
        ++g_report.simulated_ticks;
        ++ticks_on_this_cell;

        const cell_t now = game_get_pacman_cell(inout_game);

        if (playfield_are_cells_equal(now, previous))
        {
            /* Against a wall, or simply mid-step — and the difference is worth a sixth of the
             * budget, so it is asked rather than waited out (RF-019). Stranding him is this
             * caller's own doing: a leg sets one direction and lets it ride, where the played game
             * asks again on every cell and always hands him a way that is open. So the Pacman the
             * backstop below used to spend 32 ticks on is one who never occurs, and those ticks
             * were walking ghosts towards him. */
            if (pacman_is_stuck(&inout_game->pacman, game_get_playfield(inout_game)))
            {
                break;
            }

            /* The backstop stays for the case the question above cannot see: a step period so long
             * that a cell legitimately takes more ticks than this. It is now unreachable in an
             * ordinary maze and is cheap insurance against a loop that never ends. */
            if (ticks_on_this_cell >= PACMAN_LOOKAHEAD_MAX_CELL_TICKS)
            {
                break;
            }

            continue;
        }

        previous = now;
        ticks_on_this_cell = 0U;
        ++cells;
        ++g_report.simulated_cells;

        /* A death resets everyone to their starting cells, so carrying on here would be walking
         * a position that has nothing to do with the branch being priced. */
        if (game_get_lives(inout_game) != lives_at_start)
        {
            break;
        }

        if (prv_is_junction(inout_game))
        {
            break;
        }
    }

    return cells;
}

#define PACMAN_LOOKAHEAD_BRANCH_COUNT ((uint8_t)(sizeof(g_branch_order) / sizeof(g_branch_order[0])))

/* Where a level stands: level 0 on the root, every other on the clone the level above walked. */
static const game_t* prv_state_at(uint8_t in_level)
{
    return (in_level == 0U) ? &g_root : &g_clone[in_level - 1U];
}

static void prv_open_level(uint8_t in_level)
{
    g_level[in_level].next_branch = 0U;
    g_level[in_level].best_value = 0;
    g_level[in_level].best_direction = DIRECTION_NONE;
    g_level[in_level].pending_direction = DIRECTION_NONE;
    g_level[in_level].has_branch = false;
    g_level[in_level].is_entered = false;
}

/* A deepening is over. Keep its answer if it was allowed to finish, and decide what comes next.
 *
 * The rule is the one the recursive version stated in its loop and is worth restating here because
 * it is now the only place it lives: **a truncated deepening is thrown away**, because its answer
 * compares one branch that was studied against three that were not. What survives a truncation is
 * the last deepening that finished — and, if none did, the partial answer, because a caller asking
 * which way to go has to be given a legal move. */
static void prv_close_deepening(void)
{
    g_is_deepening_active = false;

    if (g_is_iteration_truncated)
    {
        g_report.was_truncated = true;

        if ((g_best == DIRECTION_NONE) && (g_level[0].best_direction != DIRECTION_NONE))
        {
            g_best = g_level[0].best_direction;
        }

        g_is_search_finished = true;

        return;
    }

    if (g_level[0].best_direction != DIRECTION_NONE)
    {
        g_best = g_level[0].best_direction;
        g_report.reached_depth = g_target_depth;
    }

    if (g_target_depth >= g_depth_ceiling)
    {
        g_is_search_finished = true;
    }
    else
    {
        ++g_target_depth;
    }
}

/* Hand a finished level's value to the level that asked for it, or end the deepening at level 0. */
static void prv_close_level(int32_t in_value)
{
    if (g_top == 0U)
    {
        prv_close_deepening();

        return;
    }

    --g_top;

    pacman_lookahead_level_t* const parent = &g_level[g_top];

    if (!parent->has_branch || (in_value > parent->best_value))
    {
        parent->has_branch = true;
        parent->best_value = in_value;
        parent->best_direction = parent->pending_direction;
    }
}

/* Walk the tree until the slice is spent or there is nothing left to do.
 *
 * \return `true` when a further slice would still change the answer */
static bool prv_advance(uint16_t in_slice_ticks)
{
    uint16_t slice_left = in_slice_ticks;

    while (!g_is_search_finished)
    {
        if (!g_is_deepening_active)
        {
            g_is_deepening_active = true;
            g_is_iteration_truncated = false;
            g_top = 0U;

            prv_open_level(0U);
        }

        const game_t* const state = prv_state_at(g_top);
        pacman_lookahead_level_t* const level = &g_level[g_top];

        if (!level->is_entered)
        {
            level->is_entered = true;

            if ((g_top >= g_target_depth) || (g_ticks_left == 0U) || (game_get_state(state) != GAME_STATE_RUNNING))
            {
                if (g_ticks_left == 0U)
                {
                    g_is_iteration_truncated = true;
                }

                prv_close_level(prv_evaluate(state, &g_root));

                continue;
            }
        }

        if (level->next_branch >= PACMAN_LOOKAHEAD_BRANCH_COUNT)
        {
            prv_close_level(level->has_branch ? level->best_value : prv_evaluate(state, &g_root));

            continue;
        }

        const direction_e direction = g_branch_order[level->next_branch];

        if (!pacman_may_step(&state->pacman, game_get_playfield(state), direction))
        {
            ++level->next_branch;

            continue;
        }

        if (g_ticks_left == 0U)
        {
            /* Out of budget with branches left unlooked-at. Close the level off and let the rule
             * in #prv_close_deepening throw the whole deepening away. */
            g_is_iteration_truncated = true;
            level->next_branch = PACMAN_LOOKAHEAD_BRANCH_COUNT;

            continue;
        }

        if (slice_left == 0U)
        {
            /* Pause *before* the branch is taken, so nothing is half-done: `next_branch` still
             * names it and the next slice picks it up as though no time had passed. */
            return true;
        }

        ++level->next_branch;

        game_clone(&g_clone[g_top], state);

        /* Before a single tick of it: the jitter is drawn from one generator shared with the game
         * being played, so a simulation that let it draw would move the real run's future. See
         * #game_freeze_timings. The root is frozen too, so this is belt and braces — and it is
         * kept, because the clone is what actually gets ticked. */
        game_freeze_timings(&g_clone[g_top]);

        const uint16_t before = g_ticks_left;
        const uint16_t walked = prv_walk_to_next_decision(&g_clone[g_top], direction);
        const uint16_t spent = (uint16_t)(before - g_ticks_left);

        /* A leg runs to its end inside the slice that started it. That is what lets the pause
         * above be free of half-finished state, and it is why a frame can overrun its slice by up
         * to one leg — about thirty ticks now that a stranded Pacman is noticed at once (RF-019),
         * against the hundreds a slice is worth. */
        slice_left = (spent >= slice_left) ? 0U : (uint16_t)(slice_left - spent);

        if (walked == 0U)
        {
            /* He could not get out of the cell that way after all — a turn the rules allow but
             * the step timing did not deliver. Nothing was simulated, so there is nothing to
             * price. */
            continue;
        }

        ++g_report.examined_legs;

        level->pending_direction = direction;
        ++g_top;

        prv_open_level(g_top);
    }

    return false;
}

/* ==========================================================================
 * pacman_lookahead - public
 * ========================================================================= */

void pacman_lookahead_restart(const game_t* in_game, uint8_t in_depth, uint16_t in_tick_budget)
{
    ASSERT(in_game != NULL);
    ASSERT(in_depth > 0U);
    ASSERT(in_depth <= PACMAN_LOOKAHEAD_MAX_DEPTH);
    ASSERT(in_tick_budget > 0U);

    memset(&g_report, 0, sizeof(g_report));

    g_best = DIRECTION_NONE;
    g_ticks_left = in_tick_budget;
    g_depth_ceiling = in_depth;

    /* **Shallow first, then deeper, keeping the last answer that was allowed to finish.** Straight
     * to the full depth is the obvious thing and it is wrong here, because the budget is smaller
     * than a full search: depth-first spends the whole allowance on the first branch, and the
     * answer is then a comparison between one way out that was studied three junctions deep and
     * three that were never looked at. That is not a theoretical worry — measured on the board's
     * own budget, the player it produced scored *worse* than one walking in a straight line.
     *
     * Iterating costs re-walking the shallow levels, which is real and is the price of an answer
     * that compares like with like. What the caller gets is the deepest complete look the budget
     * paid for, and #pacman_lookahead_report_t::reached_depth says which that was. */
    g_target_depth = 1U;
    g_is_deepening_active = false;
    g_is_iteration_truncated = false;
    g_is_search_finished = (game_get_state(in_game) != GAME_STATE_RUNNING);

    game_clone(&g_root, in_game);

    /* The root is a simulation too — it is the board every clone is copied from, and a root that
     * drew would spend the played game's numbers before any branch was tried. */
    game_freeze_timings(&g_root);
}

bool pacman_lookahead_think(uint16_t in_slice_ticks)
{
    ASSERT(in_slice_ticks > 0U);

    return prv_advance(in_slice_ticks);
}

direction_e pacman_lookahead_get_direction(void)
{
    if (game_get_state(&g_root) != GAME_STATE_RUNNING)
    {
        /* \ref DIRECTION_NONE is the honest answer to "which way" for a game that has ended. */
        return DIRECTION_NONE;
    }

    if (g_best != DIRECTION_NONE)
    {
        return g_best;
    }

    /* Not a single cell of future walked yet — the first slice of a fresh search, or a budget too
     * small to leave the square. The contract still holds: a running game gets a legal move. So it
     * hands back the first way out, which is honestly all a search this size knows. */
    for (uint8_t index = 0U; index < PACMAN_LOOKAHEAD_BRANCH_COUNT; ++index)
    {
        if (pacman_may_step(&g_root.pacman, game_get_playfield(&g_root), g_branch_order[index]))
        {
            return g_branch_order[index];
        }
    }

    return DIRECTION_NONE;
}

void pacman_lookahead_get_report(pacman_lookahead_report_t* out_report)
{
    ASSERT(out_report != NULL);

    *out_report = g_report;
}

direction_e pacman_lookahead_decide(const game_t* in_game)
{
    return pacman_lookahead_decide_within(in_game, PACMAN_LOOKAHEAD_MAX_DEPTH, PACMAN_LOOKAHEAD_DEFAULT_TICK_BUDGET,
                                          NULL);
}

direction_e pacman_lookahead_decide_within(const game_t* in_game, uint8_t in_depth, uint16_t in_tick_budget,
                                           pacman_lookahead_report_t* out_report)
{
    /* The whole budget as one slice, which is the same search the anytime one does — run without
     * ever being asked to stop. There is one implementation of the search and this is a way of
     * calling it, which is the same argument DEC-042 makes about the network that trains being the
     * network that ships. */
    pacman_lookahead_restart(in_game, in_depth, in_tick_budget);

    while (pacman_lookahead_think(in_tick_budget))
    {
        /* A slice as large as the budget cannot be interrupted by anything but the budget, so this
         * loop turns over at most once. It is a loop rather than a single call because "the slice
         * is the budget" is an arithmetic coincidence and not something to rely on. */
    }

    if (out_report != NULL)
    {
        *out_report = g_report;
    }

    return pacman_lookahead_get_direction();
}
