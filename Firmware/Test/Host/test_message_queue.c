/*
 * Unit tests for Services/message_queue.
 *
 * Pure logic, no mocks. The wrap-around is the whole point: a ring buffer that is only
 * ever exercised below capacity looks perfect and then loses or duplicates a message
 * the first time it wraps under load, which on hardware shows up as an occasional
 * missed input rather than anything reproducible.
 */
#include <stdbool.h>
#include <stdint.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "message.h"
#include "message_queue.h"
#include "unity.h"

#define TEST_CAPACITY (4U)

static message_t g_storage[TEST_CAPACITY];
static message_queue_t g_queue;

static message_t prv_make_message(uint32_t in_score)
{
    message_t message = {0};
    message_game_score_t payload = {0};

    payload.score = in_score;

    message.id = MESSAGE_ID_GAME_SCORE_UPDATED;
    message.payload_size = (uint16_t)sizeof(payload);
    (void)__builtin_memcpy(message.payload, &payload, sizeof(payload));

    return message;
}

static uint32_t prv_score_of(const message_t* const in_message)
{
    message_game_score_t payload = {0};

    (void)__builtin_memcpy(&payload, in_message->payload, sizeof(payload));

    return payload.score;
}

/* Push then pop `in_count` messages, so the ring indices advance without the queue
 * ever being full. */
static void prv_cycle(uint16_t in_count)
{
    message_t taken;

    for (uint16_t index = 0U; index < in_count; ++index)
    {
        const message_t message = prv_make_message(index);

        TEST_ASSERT_TRUE(message_queue_push(&g_queue, &message));
        TEST_ASSERT_TRUE(message_queue_pop(&g_queue, &taken));
    }
}

void setUp(void)
{
    assert_probe_begin();
    message_queue_init(&g_queue, g_storage, TEST_CAPACITY);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- empty and full ------------------------------------------------------- */

void test_a_new_queue_is_empty(void)
{
    TEST_ASSERT_TRUE(message_queue_is_empty(&g_queue));
    TEST_ASSERT_FALSE(message_queue_is_full(&g_queue));
    TEST_ASSERT_EQUAL_UINT16(0U, message_queue_get_count(&g_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, message_queue_get_free_count(&g_queue));
}

void test_popping_an_empty_queue_reports_failure(void)
{
    message_t taken;

    TEST_ASSERT_FALSE(message_queue_pop(&g_queue, &taken));
}

void test_filling_the_queue_makes_it_full(void)
{
    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        const message_t message = prv_make_message(index);

        TEST_ASSERT_TRUE(message_queue_push(&g_queue, &message));
    }

    TEST_ASSERT_TRUE(message_queue_is_full(&g_queue));
    TEST_ASSERT_EQUAL_UINT16(0U, message_queue_get_free_count(&g_queue));
}

void test_pushing_onto_a_full_queue_reports_failure_and_changes_nothing(void)
{
    const message_t overflow = prv_make_message(999U);
    message_t taken;

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        const message_t message = prv_make_message(index);

        (void)message_queue_push(&g_queue, &message);
    }

    TEST_ASSERT_FALSE(message_queue_push(&g_queue, &overflow));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, message_queue_get_count(&g_queue));

    /* The rejected message must not have displaced the oldest one. */
    TEST_ASSERT_TRUE(message_queue_pop(&g_queue, &taken));
    TEST_ASSERT_EQUAL_UINT32(0U, prv_score_of(&taken));
}

/* --- ordering and contents ------------------------------------------------ */

void test_messages_come_back_in_order(void)
{
    message_t taken;

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        const message_t message = prv_make_message(index);

        (void)message_queue_push(&g_queue, &message);
    }

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        TEST_ASSERT_TRUE(message_queue_pop(&g_queue, &taken));
        TEST_ASSERT_EQUAL_UINT32(index, prv_score_of(&taken));
    }

    TEST_ASSERT_TRUE(message_queue_is_empty(&g_queue));
}

void test_a_message_is_copied_in_so_the_caller_may_reuse_its_variable(void)
{
    message_t message = prv_make_message(7U);
    message_t taken;

    (void)message_queue_push(&g_queue, &message);

    /* Overwrite the source: a queue that stored a pointer would now be corrupt. */
    message = prv_make_message(8U);

    TEST_ASSERT_TRUE(message_queue_pop(&g_queue, &taken));
    TEST_ASSERT_EQUAL_UINT32(7U, prv_score_of(&taken));
    TEST_ASSERT_EQUAL(MESSAGE_ID_GAME_SCORE_UPDATED, taken.id);
}

/* --- wrap-around ---------------------------------------------------------- */

void test_the_queue_still_works_after_the_indices_wrap(void)
{
    message_t taken;

    /* Walk the read and write indices past the end of the storage. */
    prv_cycle(TEST_CAPACITY + 1U);

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        const message_t message = prv_make_message((uint32_t)(100U + index));

        TEST_ASSERT_TRUE(message_queue_push(&g_queue, &message));
    }

    TEST_ASSERT_TRUE(message_queue_is_full(&g_queue));

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        TEST_ASSERT_TRUE(message_queue_pop(&g_queue, &taken));
        TEST_ASSERT_EQUAL_UINT32(100U + index, prv_score_of(&taken));
    }
}

void test_a_wrapped_queue_reports_the_right_count(void)
{
    prv_cycle(TEST_CAPACITY * 3U);

    TEST_ASSERT_TRUE(message_queue_is_empty(&g_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, message_queue_get_free_count(&g_queue));
}

void test_a_capacity_of_one_still_alternates(void)
{
    static message_t single_storage[1];
    message_queue_t single;
    message_t taken;
    const message_t first = prv_make_message(1U);
    const message_t second = prv_make_message(2U);

    message_queue_init(&single, single_storage, 1U);

    TEST_ASSERT_TRUE(message_queue_push(&single, &first));
    TEST_ASSERT_FALSE(message_queue_push(&single, &second));
    TEST_ASSERT_TRUE(message_queue_pop(&single, &taken));
    TEST_ASSERT_EQUAL_UINT32(1U, prv_score_of(&taken));

    TEST_ASSERT_TRUE(message_queue_push(&single, &second));
    TEST_ASSERT_TRUE(message_queue_pop(&single, &taken));
    TEST_ASSERT_EQUAL_UINT32(2U, prv_score_of(&taken));
}

/* --- clearing ------------------------------------------------------------- */

void test_clearing_drops_everything(void)
{
    const message_t message = prv_make_message(1U);

    (void)message_queue_push(&g_queue, &message);
    (void)message_queue_push(&g_queue, &message);

    message_queue_clear(&g_queue);

    TEST_ASSERT_TRUE(message_queue_is_empty(&g_queue));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, message_queue_get_free_count(&g_queue));
}

/* --- preconditions -------------------------------------------------------- */

void test_a_queue_without_storage_asserts(void)
{
    message_queue_t bad;

    ASSERT_PROBE_EXPECT(message_queue_init(&bad, NULL, TEST_CAPACITY), "inout_storage != NULL");
}

void test_a_zero_capacity_queue_asserts(void)
{
    message_queue_t bad;

    ASSERT_PROBE_EXPECT(message_queue_init(&bad, g_storage, 0U), "in_capacity > 0U");
}

void test_pushing_a_null_message_asserts(void)
{
    ASSERT_PROBE_EXPECT(message_queue_push(&g_queue, NULL), "in_message != NULL");
}
