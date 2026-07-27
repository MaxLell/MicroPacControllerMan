/*
 * Unit tests for Services/circular_buffer.
 *
 * The wrap-around is the whole point: a ring buffer only ever exercised below capacity
 * looks perfect and then loses or duplicates an element the first time it wraps under
 * load — which on hardware shows up as an occasional missed input rather than anything
 * reproducible.
 *
 * Two element types are used on purpose, so the element-size arithmetic is exercised
 * rather than assumed.
 */
#include <stdbool.h>
#include <stdint.h>

#include "assert_probe.h"
#include "circular_buffer.h"
#include "custom_assert.h"
#include "unity.h"

#define TEST_CAPACITY (4U)
#define TEST_SINGLE_SLOT_CAPACITY (1U)

/* Element values, named so a failure message says which one is missing. */
#define TEST_VALUE_FIRST (11U)
#define TEST_VALUE_SECOND (22U)
#define TEST_VALUE_THIRD (33U)
#define TEST_VALUE_REJECTED (99U)

/* Base for a run of values pushed in a loop: element N holds TEST_VALUE_BASE + N. */
#define TEST_VALUE_BASE (100U)

/* How far to walk the indices before re-testing, so they pass the end of the storage. */
#define TEST_WRAP_CYCLES (TEST_CAPACITY + 1U)
#define TEST_MANY_WRAP_CYCLES (TEST_CAPACITY * 3U)

/*! \brief A multi-field element, to prove the buffer moves whole elements. */
typedef struct
{
    uint32_t first_field;
    uint16_t second_field;
    uint8_t third_field;
} test_element_t;

static uint32_t g_value_storage[TEST_CAPACITY];
static circular_buffer_t g_value_buffer;

static void prv_push_value(uint32_t in_value, bool in_expect_success)
{
    TEST_ASSERT_EQUAL(in_expect_success, circular_buffer_push(&g_value_buffer, &in_value));
}

static uint32_t prv_pop_value(void)
{
    uint32_t value = 0U;

    TEST_ASSERT_TRUE(circular_buffer_pop(&g_value_buffer, &value));

    return value;
}

static void prv_fill_buffer(void)
{
    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        prv_push_value(TEST_VALUE_BASE + index, true);
    }
}

/* Push then pop `in_cycles` elements, advancing both indices without ever filling up. */
static void prv_cycle_indices(uint16_t in_cycles)
{
    for (uint16_t index = 0U; index < in_cycles; ++index)
    {
        prv_push_value(TEST_VALUE_FIRST, true);
        (void)prv_pop_value();
    }
}

void setUp(void)
{
    assert_probe_begin();
    circular_buffer_init(&g_value_buffer, g_value_storage, sizeof(g_value_storage[0]),
                         TEST_CAPACITY);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- empty and full ------------------------------------------------------- */

void test_a_new_buffer_is_empty(void)
{
    TEST_ASSERT_TRUE(circular_buffer_is_empty(&g_value_buffer));
    TEST_ASSERT_FALSE(circular_buffer_is_full(&g_value_buffer));
    TEST_ASSERT_EQUAL_UINT16(0U, circular_buffer_get_count(&g_value_buffer));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, circular_buffer_get_free_count(&g_value_buffer));
}

void test_popping_an_empty_buffer_reports_failure(void)
{
    uint32_t value = 0U;

    TEST_ASSERT_FALSE(circular_buffer_pop(&g_value_buffer, &value));
}

void test_filling_the_buffer_makes_it_full(void)
{
    prv_fill_buffer();

    TEST_ASSERT_TRUE(circular_buffer_is_full(&g_value_buffer));
    TEST_ASSERT_EQUAL_UINT16(0U, circular_buffer_get_free_count(&g_value_buffer));
}

void test_pushing_onto_a_full_buffer_is_refused_and_overwrites_nothing(void)
{
    prv_fill_buffer();

    prv_push_value(TEST_VALUE_REJECTED, false);

    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, circular_buffer_get_count(&g_value_buffer));
    /* The oldest element must still be there — a rejected push must not displace it. */
    TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_BASE, prv_pop_value());
}

/* --- ordering and contents ------------------------------------------------ */

void test_elements_come_back_in_order(void)
{
    prv_fill_buffer();

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_BASE + index, prv_pop_value());
    }

    TEST_ASSERT_TRUE(circular_buffer_is_empty(&g_value_buffer));
}

void test_an_element_is_copied_in_so_the_caller_may_reuse_its_variable(void)
{
    uint32_t value = TEST_VALUE_FIRST;

    TEST_ASSERT_TRUE(circular_buffer_push(&g_value_buffer, &value));

    /* Overwrite the source: a buffer that stored a pointer would now be wrong. */
    value = TEST_VALUE_SECOND;

    TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_FIRST, prv_pop_value());
}

void test_a_multi_field_element_survives_intact(void)
{
    static test_element_t element_storage[TEST_CAPACITY];
    circular_buffer_t element_buffer;
    const test_element_t written = {TEST_VALUE_FIRST, TEST_VALUE_SECOND, TEST_VALUE_THIRD};
    test_element_t read_back = {0};

    circular_buffer_init(&element_buffer, element_storage, sizeof(element_storage[0]),
                         TEST_CAPACITY);

    TEST_ASSERT_TRUE(circular_buffer_push(&element_buffer, &written));
    TEST_ASSERT_TRUE(circular_buffer_pop(&element_buffer, &read_back));

    TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_FIRST, read_back.first_field);
    TEST_ASSERT_EQUAL_UINT16(TEST_VALUE_SECOND, read_back.second_field);
    TEST_ASSERT_EQUAL_UINT8(TEST_VALUE_THIRD, read_back.third_field);
}

/* --- wrap-around ---------------------------------------------------------- */

void test_the_buffer_still_works_after_the_indices_wrap(void)
{
    prv_cycle_indices(TEST_WRAP_CYCLES);

    prv_fill_buffer();

    TEST_ASSERT_TRUE(circular_buffer_is_full(&g_value_buffer));

    for (uint16_t index = 0U; index < TEST_CAPACITY; ++index)
    {
        TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_BASE + index, prv_pop_value());
    }
}

void test_a_wrapped_buffer_reports_the_right_count(void)
{
    prv_cycle_indices(TEST_MANY_WRAP_CYCLES);

    TEST_ASSERT_TRUE(circular_buffer_is_empty(&g_value_buffer));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, circular_buffer_get_free_count(&g_value_buffer));
}

void test_a_single_slot_buffer_still_alternates(void)
{
    static uint32_t single_storage[TEST_SINGLE_SLOT_CAPACITY];
    circular_buffer_t single_buffer;
    uint32_t first_value = TEST_VALUE_FIRST;
    uint32_t second_value = TEST_VALUE_SECOND;
    uint32_t read_back = 0U;

    circular_buffer_init(&single_buffer, single_storage, sizeof(single_storage[0]),
                         TEST_SINGLE_SLOT_CAPACITY);

    TEST_ASSERT_TRUE(circular_buffer_push(&single_buffer, &first_value));
    TEST_ASSERT_FALSE(circular_buffer_push(&single_buffer, &second_value));
    TEST_ASSERT_TRUE(circular_buffer_pop(&single_buffer, &read_back));
    TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_FIRST, read_back);

    TEST_ASSERT_TRUE(circular_buffer_push(&single_buffer, &second_value));
    TEST_ASSERT_TRUE(circular_buffer_pop(&single_buffer, &read_back));
    TEST_ASSERT_EQUAL_UINT32(TEST_VALUE_SECOND, read_back);
}

/* --- clearing ------------------------------------------------------------- */

void test_clearing_drops_everything(void)
{
    prv_fill_buffer();

    circular_buffer_clear(&g_value_buffer);

    TEST_ASSERT_TRUE(circular_buffer_is_empty(&g_value_buffer));
    TEST_ASSERT_EQUAL_UINT16(TEST_CAPACITY, circular_buffer_get_free_count(&g_value_buffer));
}

/* --- preconditions -------------------------------------------------------- */

void test_a_buffer_without_storage_asserts(void)
{
    circular_buffer_t bad_buffer;

    ASSERT_PROBE_EXPECT(circular_buffer_init(&bad_buffer, NULL, sizeof(uint32_t), TEST_CAPACITY),
                        "inout_storage != NULL");
}

void test_a_zero_element_size_asserts(void)
{
    circular_buffer_t bad_buffer;

    ASSERT_PROBE_EXPECT(circular_buffer_init(&bad_buffer, g_value_storage, 0U, TEST_CAPACITY),
                        "in_element_size > 0U");
}

void test_a_zero_capacity_asserts(void)
{
    circular_buffer_t bad_buffer;

    ASSERT_PROBE_EXPECT(
        circular_buffer_init(&bad_buffer, g_value_storage, sizeof(uint32_t), 0U),
        "in_capacity > 0U");
}

void test_pushing_a_null_element_asserts(void)
{
    ASSERT_PROBE_EXPECT(circular_buffer_push(&g_value_buffer, NULL), "in_element != NULL");
}
