/*
 * Unit tests for Services/msg_queue.
 *
 * The ring arithmetic belongs to circular_buffer and is tested there. What is left to
 * check here is the msg-typed skin: that a whole msg_t survives the round trip with its
 * topic and payload intact, and that the queue reports its state in messages.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "msg.h"
#include "msg_queue.h"
#include "unity.h"

#define TEST_CAPACITY       (4U)

#define TEST_SCORE_FIRST    (11U)
#define TEST_SCORE_SECOND   (22U)
#define TEST_SCORE_REJECTED (99U)
#define TEST_SCORE_BASE     (100U)

static msg_t g_msg_buffer[TEST_CAPACITY];
static msg_queue_t g_msg_queue;

static msg_t prv_make_score_msg(uint32_t in_score)
{
    msg_t msg = {0};
    msg_game_score_t payload = {0};

    payload.score = in_score;

    msg.id = MSG_GAME_SCORE_UPDATED;
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

static void prv_push_score(uint32_t in_score, bool in_expect_success)
{
    const msg_t msg = prv_make_score_msg(in_score);

    TEST_ASSERT_EQUAL(in_expect_success, msg_queue_push(&g_msg_queue, &msg));
}

void setUp(void)
{
    assert_probe_begin();
    msg_queue_init(&g_msg_queue, g_msg_buffer, TEST_CAPACITY);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- state reporting ------------------------------------------------------ */

void test_a_new_msg_queue_is_empty(void)
{
    TEST_ASSERT_TRUE(msg_queue_is_empty(&g_msg_queue));
    TEST_ASSERT_FALSE(msg_queue_is_full(&g_msg_queue));
    TEST_ASSERT_EQUAL_UINT16(0U, msg_queue_get_count(&g_msg_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, msg_queue_get_free_count(&g_msg_queue));
}

void test_the_msg_queue_counts_in_msgs_not_bytes(void)
{
    prv_push_score(TEST_SCORE_FIRST, true);
    prv_push_score(TEST_SCORE_SECOND, true);

    TEST_ASSERT_EQUAL_UINT16(2U, msg_queue_get_count(&g_msg_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY - 2U, msg_queue_get_free_count(&g_msg_queue));
}

void test_a_full_msg_queue_refuses_a_further_msg(void)
{
    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        prv_push_score(TEST_SCORE_BASE + index, true);
    }

    TEST_ASSERT_TRUE(msg_queue_is_full(&g_msg_queue));
    prv_push_score(TEST_SCORE_REJECTED, false);
}

void test_popping_an_empty_msg_queue_reports_failure(void)
{
    msg_t popped_msg;

    TEST_ASSERT_FALSE(msg_queue_pop(&g_msg_queue, &popped_msg));
}

/* --- the message itself --------------------------------------------------- */

void test_a_msg_keeps_its_topic_and_payload(void)
{
    msg_t popped_msg;

    prv_push_score(TEST_SCORE_FIRST, true);

    TEST_ASSERT_TRUE(msg_queue_pop(&g_msg_queue, &popped_msg));
    TEST_ASSERT_EQUAL(MSG_GAME_SCORE_UPDATED, popped_msg.id);
    TEST_ASSERT_EQUAL_UINT16(sizeof(msg_game_score_t), popped_msg.payload_size);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&popped_msg));
}

void test_msgs_come_back_in_order(void)
{
    msg_t popped_msg;

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        prv_push_score(TEST_SCORE_BASE + index, true);
    }

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        TEST_ASSERT_TRUE(msg_queue_pop(&g_msg_queue, &popped_msg));
        TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_BASE + index, prv_score_of(&popped_msg));
    }
}

void test_a_msg_is_copied_in_so_the_caller_may_reuse_its_variable(void)
{
    msg_t pushed_msg = prv_make_score_msg(TEST_SCORE_FIRST);
    msg_t popped_msg;

    TEST_ASSERT_TRUE(msg_queue_push(&g_msg_queue, &pushed_msg));

    pushed_msg = prv_make_score_msg(TEST_SCORE_SECOND);

    TEST_ASSERT_TRUE(msg_queue_pop(&g_msg_queue, &popped_msg));
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, prv_score_of(&popped_msg));
}

/* --- clearing ------------------------------------------------------------- */

void test_clearing_drops_every_queued_msg(void)
{
    prv_push_score(TEST_SCORE_FIRST, true);
    prv_push_score(TEST_SCORE_SECOND, true);

    msg_queue_clear(&g_msg_queue);

    TEST_ASSERT_TRUE(msg_queue_is_empty(&g_msg_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, msg_queue_get_free_count(&g_msg_queue));
}

/* --- preconditions -------------------------------------------------------- */

void test_a_msg_queue_without_a_buffer_asserts(void)
{
    msg_queue_t bad_queue;

    ASSERT_PROBE_EXPECT(msg_queue_init(&bad_queue, NULL, TEST_CAPACITY), "inout_storage != NULL");
}

void test_a_null_msg_queue_asserts(void)
{
    ASSERT_PROBE_EXPECT(msg_queue_init(NULL, g_msg_buffer, TEST_CAPACITY), "in_queue != NULL");
}
