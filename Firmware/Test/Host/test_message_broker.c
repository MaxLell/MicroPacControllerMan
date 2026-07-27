/*
 * Unit tests for Services/message_broker.
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
#include "custom_assert.h"
#include "message.h"
#include "message_broker.h"
/* Deliberately the real queue, not mock_message_queue.h. The broker's logic mostly *is*
 * queue orchestration, so mocking it would turn these into assertions about which
 * functions got called instead of about what a subscriber actually receives. The queue
 * has its own tests. Ceedling also needs this include to link it at all — it resolves
 * sources from what it sees in the test file, not transitively. */
#include "message_queue.h"
#include "unity.h"

#define TEST_INPUT_CAPACITY (4U)
#define TEST_OUTPUT_CAPACITY (2U)

static message_t g_input_storage[TEST_INPUT_CAPACITY];
static message_t g_first_output[TEST_OUTPUT_CAPACITY];
static message_t g_second_output[TEST_OUTPUT_CAPACITY];

static message_broker_t g_broker;
static message_subscriber_t g_first;
static message_subscriber_t g_second;

static message_t prv_make_message(message_id_e in_topic, uint32_t in_score)
{
    message_t message = {0};
    message_game_score_t payload = {0};

    payload.score = in_score;

    message.id = in_topic;
    message.payload_size = (uint16_t)sizeof(payload);
    memcpy(message.payload, &payload, sizeof(payload));

    return message;
}

static uint32_t prv_score_of(const message_t* const in_message)
{
    message_game_score_t payload = {0};

    memcpy(&payload, in_message->payload, sizeof(payload));

    return payload.score;
}

static void prv_publish(message_id_e in_topic, uint32_t in_score)
{
    const message_t message = prv_make_message(in_topic, in_score);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_broker_publish(&g_broker, &message));
}

void setUp(void)
{
    assert_probe_begin();

    memset(&g_broker, 0, sizeof(g_broker));

    message_broker_init(&g_broker, g_input_storage, TEST_INPUT_CAPACITY);
    message_subscriber_init(&g_first, g_first_output, TEST_OUTPUT_CAPACITY);
    message_subscriber_init(&g_second, g_second_output, TEST_OUTPUT_CAPACITY);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- routing -------------------------------------------------------------- */

void test_a_subscriber_receives_a_message_on_its_topic(void)
{
    message_t received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 42U);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_broker_process(&g_broker));
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_subscriber_receive(&g_first, &received));
    TEST_ASSERT_EQUAL(MESSAGE_ID_GAME_SCORE_UPDATED, received.id);
    TEST_ASSERT_EQUAL_UINT32(42U, prv_score_of(&received));
}

void test_nothing_arrives_before_the_broker_is_processed(void)
{
    message_t received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    /* Publishing is asynchronous — the publisher must not have called into the
     * subscriber, which is what the queue design is for. */
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_IDLE, message_subscriber_receive(&g_first, &received));
}

void test_a_subscriber_does_not_receive_other_topics(void)
{
    message_t received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_INPUT_DIRECTION, 1U);
    (void)message_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_IDLE, message_subscriber_receive(&g_first, &received));
}

void test_every_subscriber_of_a_topic_gets_its_own_copy(void)
{
    message_t first_received;
    message_t second_received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_OVER);
    message_broker_subscribe(&g_broker, &g_second, MESSAGE_ID_GAME_OVER);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_OVER, 7U);
    (void)message_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK,
                      message_subscriber_receive(&g_first, &first_received));
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK,
                      message_subscriber_receive(&g_second, &second_received));
    TEST_ASSERT_EQUAL_UINT32(7U, prv_score_of(&first_received));
    TEST_ASSERT_EQUAL_UINT32(7U, prv_score_of(&second_received));
}

void test_a_topic_with_no_subscriber_is_reported_not_fatal(void)
{
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    /* Normal during bring-up, so a status rather than an assertion. */
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_NO_SUBSCRIBER, message_broker_process(&g_broker));
}

void test_processing_an_empty_broker_is_idle(void)
{
    message_broker_start(&g_broker);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_IDLE, message_broker_process(&g_broker));
}

void test_process_all_drains_the_input_queue(void)
{
    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 2U);

    TEST_ASSERT_EQUAL_UINT32(2U, message_broker_process_all(&g_broker));
    TEST_ASSERT_EQUAL_UINT32(0U, message_broker_process_all(&g_broker));
}

void test_messages_keep_their_order_through_the_broker(void)
{
    message_t received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 10U);
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 20U);
    (void)message_broker_process_all(&g_broker);

    /* The output queue holds two, so both survive. */
    (void)message_subscriber_receive(&g_first, &received);
    TEST_ASSERT_EQUAL_UINT32(10U, prv_score_of(&received));
    (void)message_subscriber_receive(&g_first, &received);
    TEST_ASSERT_EQUAL_UINT32(20U, prv_score_of(&received));
}

/* --- backpressure (NFR-105) ----------------------------------------------- */

void test_publishing_onto_a_full_input_queue_is_refused_not_blocked(void)
{
    const message_t message = prv_make_message(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    message_broker_start(&g_broker);

    for (uint16_t index = 0U; index < TEST_INPUT_CAPACITY; ++index)
    {
        TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_broker_publish(&g_broker, &message));
    }

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_INPUT_FULL,
                      message_broker_publish(&g_broker, &message));
}

void test_the_headroom_check_tracks_the_input_queue(void)
{
    const message_t message = prv_make_message(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    message_broker_start(&g_broker);

    TEST_ASSERT_TRUE(message_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY));
    TEST_ASSERT_FALSE(message_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY + 1U));

    (void)message_broker_publish(&g_broker, &message);

    TEST_ASSERT_TRUE(message_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY - 1U));
    TEST_ASSERT_FALSE(message_broker_has_input_space(&g_broker, TEST_INPUT_CAPACITY));
}

void test_room_frees_up_again_once_the_broker_is_processed(void)
{
    const message_t message = prv_make_message(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    for (uint16_t index = 0U; index < TEST_INPUT_CAPACITY; ++index)
    {
        (void)message_broker_publish(&g_broker, &message);
    }

    (void)message_broker_process(&g_broker);

    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_broker_publish(&g_broker, &message));
}

/* --- slow-consumer isolation ---------------------------------------------- */

void test_a_slow_subscriber_loses_messages_and_they_are_counted(void)
{
    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    /* Three messages into an output queue that holds two, with nobody draining it. */
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 2U);
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 3U);
    (void)message_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(1U, message_subscriber_get_dropped_count(&g_first));
    TEST_ASSERT_EQUAL_UINT32(1U, message_broker_get_dropped_count(&g_broker));
}

void test_a_slow_subscriber_does_not_starve_a_fast_one(void)
{
    message_t received;

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_subscribe(&g_broker, &g_second, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);

    /* Fill the first subscriber's queue and leave it full. */
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);
    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 2U);
    (void)message_broker_process_all(&g_broker);

    /* The second keeps up. */
    (void)message_subscriber_receive(&g_second, &received);
    (void)message_subscriber_receive(&g_second, &received);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 3U);
    (void)message_broker_process_all(&g_broker);

    /* The third message was dropped for the stalled subscriber but delivered to the
     * one that was keeping up — and the broker itself never stalled. */
    TEST_ASSERT_EQUAL_UINT32(1U, message_subscriber_get_dropped_count(&g_first));
    TEST_ASSERT_EQUAL_UINT32(0U, message_subscriber_get_dropped_count(&g_second));
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_subscriber_receive(&g_second, &received));
    TEST_ASSERT_EQUAL_UINT32(3U, prv_score_of(&received));
}

/* --- two instances (FR-110) ----------------------------------------------- */

void test_two_brokers_do_not_share_anything(void)
{
    static message_t other_input[TEST_INPUT_CAPACITY];
    static message_t other_output[TEST_OUTPUT_CAPACITY];
    message_broker_t other_broker;
    message_subscriber_t other_subscriber;
    message_t received;

    memset(&other_broker, 0, sizeof(other_broker));
    message_broker_init(&other_broker, other_input, TEST_INPUT_CAPACITY);
    message_subscriber_init(&other_subscriber, other_output, TEST_OUTPUT_CAPACITY);

    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_subscribe(&other_broker, &other_subscriber, MESSAGE_ID_GAME_SCORE_UPDATED);
    message_broker_start(&g_broker);
    message_broker_start(&other_broker);

    prv_publish(MESSAGE_ID_GAME_SCORE_UPDATED, 5U);
    (void)message_broker_process_all(&g_broker);

    /* Same topic, same subscriber shape — but the other instance saw nothing. */
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_OK, message_subscriber_receive(&g_first, &received));
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_IDLE,
                      message_subscriber_receive(&other_subscriber, &received));
    TEST_ASSERT_EQUAL(MESSAGE_BROKER_STATUS_IDLE, message_broker_process(&other_broker));
}

/* --- preconditions -------------------------------------------------------- */

void test_subscribing_the_same_subscriber_to_a_topic_twice_asserts(void)
{
    message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_OVER);

    ASSERT_PROBE_EXPECT(message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_OVER),
                        "inout_broker->topics[in_topic][slot] != inout_subscriber");
}

void test_subscribing_to_a_reserved_topic_asserts(void)
{
    ASSERT_PROBE_EXPECT(message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_NONE),
                        "in_topic > MESSAGE_ID_NONE");
}

void test_publishing_before_start_asserts(void)
{
    const message_t message = prv_make_message(MESSAGE_ID_GAME_SCORE_UPDATED, 1U);

    ASSERT_PROBE_EXPECT(message_broker_publish(&g_broker, &message),
                        "in_broker->is_started");
}

void test_exhausting_a_topic_asserts(void)
{
    static message_subscriber_t extra[MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC];
    static message_t extra_storage[MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC][TEST_OUTPUT_CAPACITY];

    for (uint16_t index = 0U; index < MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++index)
    {
        message_subscriber_init(&extra[index], extra_storage[index], TEST_OUTPUT_CAPACITY);
        message_broker_subscribe(&g_broker, &extra[index], MESSAGE_ID_GAME_OVER);
    }

    ASSERT_PROBE_EXPECT(message_broker_subscribe(&g_broker, &g_first, MESSAGE_ID_GAME_OVER),
                        "false");
}
