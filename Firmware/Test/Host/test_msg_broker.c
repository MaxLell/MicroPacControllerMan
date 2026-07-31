/*
 * Unit tests for Services/msg_broker.
 *
 * These are the reason the broker is worth unit-testing at all: backpressure and
 * slow-consumer isolation are load conditions. On hardware they happen rarely, at the
 * worst moment, and leave no trace — a dropped input, a frame that never rendered.
 * Here they are just arithmetic.
 *
 * Two broker instances run side by side in one test to prove the object-ness (FR-110),
 * which a singleton design could not do at all.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
/* circular_buffer sits two includes away (broker -> queue -> buffer). Ceedling only
 * looks at this file's includes, so without it the link fails on circular_buffer_push. */
#include "circular_buffer.h"
#include "custom_assert.h"
#include "msg.h"
#include "msg_broker.h"
/* Deliberately the real queue, not mock_msg_queue.h. The broker's logic mostly *is*
 * queue orchestration, so mocking it would turn these into assertions about which
 * functions got called instead of about what a subscriber actually receives. The queue
 * and the ring buffer under it have their own tests. Ceedling also needs this include to
 * link it at all — it resolves sources from what it sees in the test file, not
 * transitively. */
#include "msg_queue.h"
#include "unity.h"

/* One input slot more than the output queues, so a full output can be provoked without
 * the input queue filling first. */
#define TEST_INPUT_CAPACITY  (4U)
#define TEST_OUTPUT_CAPACITY (2U)

/* Payload values, named so a failure message says which message went missing. */
#define TEST_SCORE_FIRST     (11U)
#define TEST_SCORE_SECOND    (22U)
#define TEST_SCORE_THIRD     (33U)
#define TEST_SCORE_ANY       (44U)

#define TEST_NO_DROPS        (0U)
#define TEST_ONE_DROP        (1U)

static msg_t g_input_msg_buffer[TEST_INPUT_CAPACITY];
static msg_t g_first_output_msg_buffer[TEST_OUTPUT_CAPACITY];
static msg_t g_second_output_msg_buffer[TEST_OUTPUT_CAPACITY];

static msg_broker_t g_broker;
static msg_subscriber_t g_first_subscriber;
static msg_subscriber_t g_second_subscriber;

static msg_t prv_make_score_msg(msg_id_e in_topic, uint32_t in_score)
{
    msg_t msg = {0};
    msg_game_score_t payload = {0};

    payload.score = in_score;

    msg.id = in_topic;
    msg.payload_size = (uint16_t)sizeof(payload);
    memcpy(msg.payload, &payload, sizeof(payload));

    return msg;
}

static uint32_t prv_score_of(const msg_t* const in_msg)
{
    msg_game_score_t payload = {0};

    memcpy(&payload, in_msg->payload, sizeof(payload));

    return payload.score;
}

static void prv_publish_score(msg_id_e in_topic, uint32_t in_score)
{
    const msg_t msg = prv_make_score_msg(in_topic, in_score);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_publish(&g_broker, &msg));
}

/* Fill a subscriber's output queue to the brim and leave it there. */
static void prv_stall_first_subscriber(void)
{
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);
}

void setUp(void)
{
    assert_probe_begin();

    memset(&g_broker, 0, sizeof(g_broker));

    msg_broker_init(&g_broker, g_input_msg_buffer, TEST_INPUT_CAPACITY);
    msg_subscriber_init(&g_first_subscriber, g_first_output_msg_buffer, TEST_OUTPUT_CAPACITY);
    msg_subscriber_init(&g_second_subscriber, g_second_output_msg_buffer, TEST_OUTPUT_CAPACITY);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- routing -------------------------------------------------------------- */

void test_a_subscriber_receives_a_msg_on_its_topic(void)
{
    msg_t received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_process(&g_broker));
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_subscriber_receive(&g_first_subscriber, &received_msg));
    TEST_ASSERT_EQUAL(MSG_GAME_SCORE_UPDATED, received_msg.id);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&received_msg));
}

void test_nothing_arrives_before_the_broker_is_processed(void)
{
    msg_t received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);

    /* Publishing is asynchronous — the publisher must not have called into the
     * subscriber, which is what the queue design is for. */
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_IDLE, msg_subscriber_receive(&g_first_subscriber, &received_msg));
}

void test_a_subscriber_does_not_receive_other_topics(void)
{
    msg_t received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_INPUT_DIRECTION, TEST_SCORE_ANY);
    (void)msg_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_IDLE, msg_subscriber_receive(&g_first_subscriber, &received_msg));
}

void test_every_subscriber_of_a_topic_gets_its_own_copy(void)
{
    msg_t first_received_msg;
    msg_t second_received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_OVER);
    msg_broker_subscribe(&g_broker, &g_second_subscriber, MSG_GAME_OVER);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_OVER, TEST_SCORE_FIRST);
    (void)msg_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_subscriber_receive(&g_first_subscriber, &first_received_msg));
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_subscriber_receive(&g_second_subscriber, &second_received_msg));
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&first_received_msg));
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&second_received_msg));
}

void test_a_topic_with_no_subscriber_is_reported_not_fatal(void)
{
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_ANY);

    /* Normal during bring-up, so a status rather than an assertion. */
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_NO_SUBSCRIBER, msg_broker_process(&g_broker));
}

void test_processing_an_empty_broker_is_idle(void)
{
    msg_broker_start(&g_broker);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_IDLE, msg_broker_process(&g_broker));
}

void test_process_all_drains_the_input_queue(void)
{
    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);

    TEST_ASSERT_EQUAL_UINT32(2U, msg_broker_process_all(&g_broker));
    TEST_ASSERT_EQUAL_UINT32(0U, msg_broker_process_all(&g_broker));
}

void test_msgs_keep_their_order_through_the_broker(void)
{
    msg_t received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    /* The output queue holds both, so neither is lost. */
    (void)msg_subscriber_receive(&g_first_subscriber, &received_msg);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&received_msg));
    (void)msg_subscriber_receive(&g_first_subscriber, &received_msg);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_SECOND, prv_score_of(&received_msg));
}

/* --- backpressure (NFR-105) ----------------------------------------------- */

void test_publishing_onto_a_full_input_queue_is_refused_not_blocked(void)
{
    const msg_t msg = prv_make_score_msg(MSG_GAME_SCORE_UPDATED, TEST_SCORE_ANY);

    msg_broker_start(&g_broker);

    for (uint16_t index = 0U; index < TEST_INPUT_CAPACITY; ++index)
    {
        TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_publish(&g_broker, &msg));
    }

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_INPUT_FULL, msg_broker_publish(&g_broker, &msg));
}

void test_the_headroom_check_tracks_the_input_queue(void)
{
    const msg_t msg = prv_make_score_msg(MSG_GAME_SCORE_UPDATED, TEST_SCORE_ANY);

    msg_broker_start(&g_broker);

    TEST_ASSERT_TRUE(msg_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY));
    TEST_ASSERT_FALSE(msg_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY + 1U));

    (void)msg_broker_publish(&g_broker, &msg);

    TEST_ASSERT_TRUE(msg_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY - 1U));
    TEST_ASSERT_FALSE(msg_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY));
}

void test_room_frees_up_again_once_the_broker_is_processed(void)
{
    const msg_t msg = prv_make_score_msg(MSG_GAME_SCORE_UPDATED, TEST_SCORE_ANY);

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    for (uint16_t index = 0U; index < TEST_INPUT_CAPACITY; ++index)
    {
        (void)msg_broker_publish(&g_broker, &msg);
    }

    (void)msg_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_publish(&g_broker, &msg));
}

/* --- slow-consumer isolation ---------------------------------------------- */

void test_a_slow_subscriber_loses_msgs_and_they_are_counted(void)
{
    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    /* One message more than the output queue holds, with nobody draining it. */
    prv_stall_first_subscriber();
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_THIRD);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_DROP, msg_subscriber_get_dropped_msg_count(&g_first_subscriber));
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_DROP, msg_broker_get_dropped_msg_count(&g_broker));
}

void test_a_slow_subscriber_does_not_starve_a_fast_one(void)
{
    msg_t received_msg;

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_subscribe(&g_broker, &g_second_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_stall_first_subscriber();

    /* The second subscriber keeps up, so its queue has room again. */
    (void)msg_subscriber_receive(&g_second_subscriber, &received_msg);
    (void)msg_subscriber_receive(&g_second_subscriber, &received_msg);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_THIRD);
    (void)msg_broker_process_all(&g_broker);

    /* Dropped for the stalled subscriber, delivered to the one keeping up — and the
     * broker itself never stalled. */
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_DROP, msg_subscriber_get_dropped_msg_count(&g_first_subscriber));
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_DROPS, msg_subscriber_get_dropped_msg_count(&g_second_subscriber));
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_subscriber_receive(&g_second_subscriber, &received_msg));
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_THIRD, prv_score_of(&received_msg));
}

/* --- two instances (FR-110) ----------------------------------------------- */

void test_two_brokers_do_not_share_anything(void)
{
    static msg_t other_input_msg_buffer[TEST_INPUT_CAPACITY];
    static msg_t other_output_msg_buffer[TEST_OUTPUT_CAPACITY];
    msg_broker_t other_broker;
    msg_subscriber_t other_subscriber;
    msg_t received_msg;

    memset(&other_broker, 0, sizeof(other_broker));
    msg_broker_init(&other_broker, other_input_msg_buffer, TEST_INPUT_CAPACITY);
    msg_subscriber_init(&other_subscriber, other_output_msg_buffer, TEST_OUTPUT_CAPACITY);

    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_subscribe(&other_broker, &other_subscriber, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);
    msg_broker_start(&other_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    (void)msg_broker_process_all(&g_broker);

    /* Same topic, same subscriber shape — but the other instance saw nothing. */
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_subscriber_receive(&g_first_subscriber, &received_msg));
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_IDLE, msg_subscriber_receive(&other_subscriber, &received_msg));
    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_IDLE, msg_broker_process(&other_broker));
}

/* --- preconditions -------------------------------------------------------- */

void test_subscribing_the_same_subscriber_to_a_topic_twice_asserts(void)
{
    msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_OVER);

    ASSERT_PROBE_EXPECT(msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_OVER),
                        "inout_broker->topics[in_topic][slot] != inout_subscriber");
}

void test_subscribing_to_a_reserved_topic_asserts(void)
{
    ASSERT_PROBE_EXPECT(msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_NONE), "in_topic > MSG_NONE");
}

void test_publishing_before_start_asserts(void)
{
    const msg_t msg = prv_make_score_msg(MSG_GAME_SCORE_UPDATED, TEST_SCORE_ANY);

    ASSERT_PROBE_EXPECT(msg_broker_publish(&g_broker, &msg), "in_broker->is_started");
}

void test_exhausting_a_topic_asserts(void)
{
    static msg_subscriber_t extra_subscribers[MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC];
    static msg_t extra_msg_buffers[MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC][TEST_OUTPUT_CAPACITY];

    for (uint16_t index = 0U; index < MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++index)
    {
        msg_subscriber_init(&extra_subscribers[index], extra_msg_buffers[index], TEST_OUTPUT_CAPACITY);
        msg_broker_subscribe(&g_broker, &extra_subscribers[index], MSG_GAME_OVER);
    }

    ASSERT_PROBE_EXPECT(msg_broker_subscribe(&g_broker, &g_first_subscriber, MSG_GAME_OVER), "false");
}
