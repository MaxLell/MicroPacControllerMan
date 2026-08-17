/*
 * record_states.c
 *
 * Records the state set VT-INT-024 replays on the target, and what the host decided about each
 * ([M6 §9](../../Docu/Design/M6-Pacman-AI.md), FR-039).
 *
 *   ./build-host/pacman_ai_record > Test/Target/scripts/ott_ai_equivalence_states.c
 *
 * Written in **C** rather than Python, and that is the whole point: it calls the same
 * `pacman_ai_decide` the target calls, on the same structs, so the recorded answer is the host
 * build's own answer and not a re-derivation of it. A Python recorder would have had to mirror
 * `msg_game_state_t`, which is exactly the copy that would drift.
 *
 * Four situations, because they are the four places the feature extractor has something
 * interesting to do: ordinary play, frightened mode, inside a tunnel, and the step after a life
 * was lost. The **normal maze** is played first, since that is the only maze the AI may be handed
 * control in (FR-040); generated mazes are then tried in seed order for any situation that one
 * episode of it did not happen to contain. Either way the set is whatever the game actually
 * produced rather than a set of states hand-built to be convenient.
 *
 * What is emitted per case is the maze, the state, and the direction this build chose. The maze is
 * needed because the observation asks the playfield about walls and about how much of the level is
 * left; the pellets are *not* emitted, because the state's own bitmaps already say where they are
 * and the target rebuilds the playfield from them.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ai_weights.h"
#include "game.h"
#include "game_session.h"
#include "msg.h"
#include "pacman_ai.h"
#include "playfield.h"

#define STEP_MS             (GAME_SESSION_FRAME_PERIOD_MS)
#define DECISION_TIMEOUT_MS (500U)
#define MAX_DECISIONS       (20000U)
#define MAX_SEEDS           (200U)

typedef enum
{
    CASE_ORDINARY = 0,
    CASE_FRIGHTENED,
    CASE_TUNNEL,
    CASE_LIFE_JUST_LOST,
    CASE_COUNT
} recorded_case_e;

static const char* const g_case_names[CASE_COUNT] = {
    "ordinary play",
    "frightened mode",
    "in a tunnel",
    "a life just lost",
};

typedef struct
{
    bool is_recorded;
    uint8_t level;
    playfield_map_t map;
    msg_game_state_t state;
    direction_e chosen;

    /*! \brief Which episode this came out of, for the comment above it in the generated file. */
    char source[32];
} recorded_case_t;

static recorded_case_t g_cases[CASE_COUNT];
static game_t g_game;

/*! \brief What the episode being played is, in words. Written once per episode and copied into
 *         every case it produces, so the two cannot disagree. */
static char g_episode_source[32];

static void prv_record(recorded_case_e in_which, const msg_game_state_t* const in_state)
{
    recorded_case_t* const entry = &g_cases[in_which];

    if (entry->is_recorded)
    {
        return;
    }

    entry->is_recorded = true;
    (void)snprintf(entry->source, sizeof(entry->source), "%s", g_episode_source);
    entry->level = in_state->level;
    entry->map = *game_get_maze(&g_game);
    entry->state = *in_state;

    /* The host's answer, taken here rather than recomputed when the file is written: it must be
     * this build's own decision about this exact state. */
    entry->chosen = pacman_ai_decide(in_state, game_get_playfield(&g_game));
}

/* Which situation a state is, or CASE_COUNT for none of them. */
static recorded_case_e prv_classify(const msg_game_state_t* const in_state, uint8_t in_lives_before)
{
    const cell_t pacman = {.x = (int16_t)in_state->pacman.column, .y = (int16_t)in_state->pacman.row};

    if (in_state->lives < in_lives_before)
    {
        return CASE_LIFE_JUST_LOST;
    }

    if (in_state->frightened_ghosts != 0U)
    {
        return CASE_FRIGHTENED;
    }

    if (playfield_is_tunnel(game_get_playfield(&g_game), pacman))
    {
        return CASE_TUNNEL;
    }

    /* Ordinary play is deliberately narrow: a few decisions in, so the ghosts are out of the house
     * and the maze is nearly full — the commonest state there is, and therefore the one a
     * divergence would show up in first. */
    if ((in_state->level == 1U) && (in_state->score > 0U) && (in_state->frightened_ghosts == 0U))
    {
        return CASE_ORDINARY;
    }

    return CASE_COUNT;
}

static bool prv_have_every_case(void)
{
    for (uint8_t index = 0U; index < (uint8_t)CASE_COUNT; ++index)
    {
        if (!g_cases[index].is_recorded)
        {
            return false;
        }
    }

    return true;
}

/* Play one episode with the trained network, recording whatever situations turn up in it.
 *
 * `in_seed` of `0` means the normal maze — the one the AI is actually allowed to play (FR-040), and
 * therefore where these states should come from. A generated maze is still a legitimate source for
 * a case the normal one did not produce: FR-039 is about the *port* agreeing with the host, and a
 * state is a state whichever maze it arose in. */
static void prv_play_one_episode(uint32_t in_seed)
{
    msg_game_state_t state;

    game_init(&g_game);

    if (in_seed == 0U)
    {
        playfield_map_t normal_maze;

        playfield_get_arcade_map(&normal_maze);
        game_start_on_map(&g_game, &normal_maze);
        (void)snprintf(g_episode_source, sizeof(g_episode_source), "the normal maze");
    }
    else
    {
        game_start(&g_game, in_seed);
        (void)snprintf(g_episode_source, sizeof(g_episode_source), "generated maze, seed %lu", (unsigned long)in_seed);
    }

    for (uint32_t decision = 0U; decision < MAX_DECISIONS; ++decision)
    {
        game_get_state_message(&g_game, &state);

        const uint8_t lives_before = state.lives;
        const uint8_t column_before = state.pacman.column;
        const uint8_t row_before = state.pacman.row;
        const recorded_case_e which = prv_classify(&state, lives_before);

        if (which != CASE_COUNT)
        {
            prv_record(which, &state);
        }

        game_set_direction(&g_game, pacman_ai_decide(&state, game_get_playfield(&g_game)));

        uint32_t elapsed_ms = 0U;
        bool has_arrived = false;

        while ((elapsed_ms < DECISION_TIMEOUT_MS) && !has_arrived)
        {
            game_tick(&g_game, STEP_MS);
            elapsed_ms += STEP_MS;

            if (game_get_state(&g_game) != GAME_STATE_RUNNING)
            {
                return;
            }

            game_get_state_message(&g_game, &state);
            has_arrived = (state.pacman.column != column_before) || (state.pacman.row != row_before);
        }

        /* Losing a life is only visible by comparing with the step before, so it is checked here
         * rather than at the top of the loop. */
        if (state.lives < lives_before)
        {
            prv_record(CASE_LIFE_JUST_LOST, &state);
        }

        if (prv_have_every_case())
        {
            return;
        }
    }
}

static void prv_print_bitmap(const char* const in_name, const uint8_t* const in_bytes)
{
    printf("            .%s = {", in_name);

    for (uint16_t index = 0U; index < MSG_CELL_BITMAP_BYTES; ++index)
    {
        /* The separator goes *before* every value but the first, and a line break replaces the space
         * rather than the comma — dropping the comma at the line break is what the first version of
         * this did, and it produced 109 bytes of something that was not C. */
        if (index > 0U)
        {
            printf(",");
        }

        printf("%s0x%02XU", ((index % 12U) == 0U) ? "\n                " : " ", (unsigned)in_bytes[index]);
    }

    printf("},\n");
}

/* One actor. `in_prefix` carries the indentation and the field name, or just the indentation for an
 * array element — a designated initialiser has no name inside an array. */
static void prv_print_actor(const char* const in_prefix, const msg_actor_t* const in_actor)
{
    printf("%s{.column = %uU, .row = %uU, .direction = %uU, .progress = %uU},\n", in_prefix, (unsigned)in_actor->column,
           (unsigned)in_actor->row, (unsigned)in_actor->direction, (unsigned)in_actor->progress);
}

static void prv_print_case(const recorded_case_t* const in_entry, uint8_t in_index)
{
    printf("    /* --- %s: %s, level %u ------------------------------------- */\n", g_case_names[in_index],
           in_entry->source, (unsigned)in_entry->level);
    printf("    {\n        .what = \"%s\",\n", g_case_names[in_index]);

    printf("        .map = {.rows = {");
    for (uint8_t row = 0U; row < PLAYFIELD_HEIGHT; ++row)
    {
        printf("\n            \"%s\",", in_entry->map.rows[row]);
    }
    printf("}},\n");

    printf("        .state =\n        {\n");
    prv_print_actor("            .pacman = ", &in_entry->state.pacman);
    printf("            .ghosts =\n            {\n");
    for (uint8_t ghost = 0U; ghost < MSG_GHOST_COUNT; ++ghost)
    {
        prv_print_actor("                ", &in_entry->state.ghosts[ghost]);
    }
    printf("            },\n");
    prv_print_bitmap("has_pellet", in_entry->state.has_pellet);
    prv_print_bitmap("is_power", in_entry->state.is_power);
    printf("            .score = %luUL,\n", (unsigned long)in_entry->state.score);
    printf("            .lives = %uU,\n", (unsigned)in_entry->state.lives);
    printf("            .level = %uU,\n", (unsigned)in_entry->state.level);
    printf("            .frightened_ghosts = 0x%02XU,\n", (unsigned)in_entry->state.frightened_ghosts);
    printf("            .are_frightened_ghosts_flashing = %s,\n",
           in_entry->state.are_frightened_ghosts_flashing ? "true" : "false");
    printf("        },\n");
    printf("        .expected_direction = %uU,\n", (unsigned)in_entry->chosen);
    printf("    },\n");
}

int main(void)
{
    if (!pacman_ai_is_available())
    {
        fprintf(stderr, "the weight table in App/pacman_ai/ai_weights.c cannot be evaluated\n");

        return 1;
    }

    /* The normal maze first, because that is the maze the agent plays. One episode of it is one
     * trajectory, so it may not contain all four situations; generated mazes then fill in whatever
     * is missing rather than the set being left short. */
    prv_play_one_episode(0U);

    for (uint32_t seed = 1U; (seed <= MAX_SEEDS) && !prv_have_every_case(); ++seed)
    {
        prv_play_one_episode(seed);
    }

    for (uint8_t index = 0U; index < (uint8_t)CASE_COUNT; ++index)
    {
        if (!g_cases[index].is_recorded)
        {
            fprintf(stderr, "no episode in the normal maze or %u seeds produced \"%s\"\n", (unsigned)MAX_SEEDS,
                    g_case_names[index]);

            return 1;
        }
    }

    printf("/*\n"
           " * ott_ai_equivalence_states.c\n"
           " *\n"
           " * GENERATED — do not edit. Written by Training/record_states.c, which is built as\n"
           " * `pacman_ai_record` in the host configuration:\n"
           " *\n"
           " *   ./build-host/pacman_ai_record > Test/Target/scripts/ott_ai_equivalence_states.c\n"
           " *\n"
           " * The states the host played through, and the direction the host build chose about each of\n"
           " * them. The target replays them and must agree (FR-039, VT-INT-024).\n"
           " *\n"
           " * Tied to one weight table: the expected directions belong to the network below, so\n"
           " * re-exporting weights means re-recording these. The digest is carried along and the test\n"
           " * refuses to run when it does not match, which is what stops a stale file being read as a\n"
           " * porting fault.\n"
           " */\n\n");
    printf("#include \"ott_ai_equivalence_states.h\"\n\n");
    /* The digest as a *literal*, not as `AI_WEIGHTS_DIGEST`: the point is to record which table
     * these answers came from, and a macro would be re-expanded against whatever table the target
     * is built with — so it would always agree and would catch nothing. */
    printf("const char g_ai_equivalence_digest[] = \"%s\";\n\n", AI_WEIGHTS_DIGEST);
    printf("const ai_equivalence_case_t g_ai_equivalence_cases[] = {\n");

    for (uint8_t index = 0U; index < (uint8_t)CASE_COUNT; ++index)
    {
        prv_print_case(&g_cases[index], index);
    }

    printf("};\n\n");
    printf("const size_t g_ai_equivalence_case_count = %u;\n", (unsigned)CASE_COUNT);

    return 0;
}
