/*
 * Unit tests for App/score.
 *
 * Score is the first module that is reached *only* by messages, so these also demonstrate
 * what the game's internal broker bought: the scoring rules are verified with no maze, no
 * positions and no game running — just events on a bus.
 *
 * The ghost bonus chain is the part worth the effort. 200/400/800/1600 within one
 * frightened window, restarting on the next power pellet (§10.5/§10.6). Off by one link
 * and a good run scores wrong in a way nobody would notice while playing.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "active_object.h"
#include "assert_probe.h"
/* Ceedling links from this file's includes only, not transitively. */
#include "circular_buffer.h"
#include "custom_assert.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "score.h"
#include "unity.h"

#define BROKER_INPUT_CAPACITY (16U)

#define GHOST_POINTS_FIRST    (200U)
#define GHOST_POINTS_SECOND   (400U)
#define GHOST_POINTS_THIRD    (800U)
#define GHOST_POINTS_FOURTH   (1600U)
#define GHOST_POINTS_ALL_FOUR (GHOST_POINTS_FIRST + GHOST_POINTS_SECOND + GHOST_POINTS_THIRD + GHOST_POINTS_FOURTH)

static msg_t g_broker_input_msg_buffer[BROKER_INPUT_CAPACITY];
static msg_broker_t g_broker;
static score_t g_score;

/* Publish an event and let it reach Score, the way the orchestrator does each tick. */
static void prv_deliver(msg_id_e in_topic, const void* in_payload, uint16_t in_payload_size)
{
    msg_t msg = {0};

    msg.id = in_topic;
    msg.payload_size = in_payload_size;

    if (in_payload != NULL)
    {
        memcpy(msg.payload, in_payload, in_payload_size);
    }

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_publish(&g_broker, &msg));
    (void)msg_broker_process_all(&g_broker);
    (void)score_process(&g_score);
}

static void prv_eat_pellet(bool in_is_power)
{
    msg_pellet_eaten_t payload = {0};

    payload.is_power_pellet = in_is_power;

    prv_deliver(MSG_GAME_PELLET_EATEN, &payload, (uint16_t)sizeof(payload));
}

static void prv_eat_ghost(void)
{
    prv_deliver(MSG_GAME_GHOST_EATEN, NULL, 0U);
}

static void prv_start_frightened(void)
{
    prv_deliver(MSG_GAME_FRIGHTENED_STARTED, NULL, 0U);
}

void setUp(void)
{
    assert_probe_begin();

    memset(&g_broker, 0, sizeof(g_broker));
    msg_broker_init(&g_broker, g_broker_input_msg_buffer, BROKER_INPUT_CAPACITY);

    score_init(&g_score, &g_broker);

    msg_broker_start(&g_broker);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- pellets (§10.6) ----------------------------------------------------- */

void test_a_new_score_starts_at_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, score_get_total(&g_score));
}

void test_nothing_is_scored_until_the_events_are_processed(void)
{
    msg_t msg = {0};
    msg_pellet_eaten_t payload = {0};

    msg.id = MSG_GAME_PELLET_EATEN;
    msg.payload_size = (uint16_t)sizeof(payload);
    memcpy(msg.payload, &payload, sizeof(payload));

    (void)msg_broker_publish(&g_broker, &msg);

    /* Publishing is asynchronous: Score has not been given the message yet. */
    TEST_ASSERT_EQUAL_UINT32(0U, score_get_total(&g_score));
}

void test_a_pellet_scores_ten(void)
{
    prv_eat_pellet(false);

    TEST_ASSERT_EQUAL_UINT32(SCORE_PELLET_POINTS, score_get_total(&g_score));
}

void test_a_power_pellet_scores_fifty(void)
{
    prv_eat_pellet(true);

    TEST_ASSERT_EQUAL_UINT32(SCORE_POWER_PELLET_POINTS, score_get_total(&g_score));
}

void test_pellets_accumulate(void)
{
    prv_eat_pellet(false);
    prv_eat_pellet(false);
    prv_eat_pellet(true);

    TEST_ASSERT_EQUAL_UINT32((2U * SCORE_PELLET_POINTS) + SCORE_POWER_PELLET_POINTS, score_get_total(&g_score));
}

/* --- the ghost bonus chain (§10.5/§10.6) --------------------------------- */

void test_the_ghost_chain_doubles_for_each_ghost_in_one_window(void)
{
    prv_start_frightened();

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST, score_get_total(&g_score));

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST + GHOST_POINTS_SECOND, score_get_total(&g_score));

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST + GHOST_POINTS_SECOND + GHOST_POINTS_THIRD, score_get_total(&g_score));

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_ALL_FOUR, score_get_total(&g_score));
}

void test_the_chain_restarts_with_the_next_power_pellet(void)
{
    prv_start_frightened();
    prv_eat_ghost();
    prv_eat_ghost();

    prv_start_frightened();

    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST, score_get_next_ghost_points(&g_score));

    prv_eat_ghost();

    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST + GHOST_POINTS_SECOND + GHOST_POINTS_FIRST, score_get_total(&g_score));
}

void test_the_chain_does_not_climb_past_the_fourth_ghost(void)
{
    /* There are only four ghosts, so a fifth in one window would mean the chain was never
     * reset. Cap it rather than let the bonus run away. */
    prv_start_frightened();

    for (uint8_t index = 0U; index < (SCORE_GHOST_CHAIN_LENGTH + 2U); ++index)
    {
        prv_eat_ghost();
    }

    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_ALL_FOUR + (2U * GHOST_POINTS_FOURTH), score_get_total(&g_score));
}

void test_the_next_ghost_value_tracks_the_chain(void)
{
    prv_start_frightened();

    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_FIRST, score_get_next_ghost_points(&g_score));

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_SECOND, score_get_next_ghost_points(&g_score));

    prv_eat_ghost();
    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_THIRD, score_get_next_ghost_points(&g_score));
}

void test_pellets_do_not_disturb_the_ghost_chain(void)
{
    prv_start_frightened();
    prv_eat_ghost();

    prv_eat_pellet(false);

    TEST_ASSERT_EQUAL_UINT32(GHOST_POINTS_SECOND, score_get_next_ghost_points(&g_score));
}

/* --- isolation ----------------------------------------------------------- */

void test_score_ignores_topics_it_did_not_subscribe_to(void)
{
    msg_t msg = {0};

    /* A system topic on the same bus: Score must not even receive it, so the handler's
     * "unknown topic" assertion is never reached. */
    msg.id = MSG_RENDER_FRAME;
    msg.payload_size = 0U;

    (void)msg_broker_publish(&g_broker, &msg);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(0U, score_process(&g_score));
    TEST_ASSERT_EQUAL_UINT32(0U, score_get_total(&g_score));
}

void test_processing_with_nothing_waiting_is_harmless(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, score_process(&g_score));
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_score_asserts(void)
{
    ASSERT_PROBE_EXPECT(score_init(NULL, &g_broker), "inout_score != NULL");
}

void test_a_null_broker_asserts(void)
{
    score_t bad_score;

    ASSERT_PROBE_EXPECT(score_init(&bad_score, NULL), "inout_broker != NULL");
}
