/*
 * test_high_score.c
 *
 * The three best scores, and what happens when the bytes they were stored in are not
 * trustworthy.
 *
 * `flash_bsp` is mocked, which is the point: the interesting cases are an erased page, a
 * page from another build and a page a power cut caught halfway, and all three are states
 * a real flash chip would take a long afternoon to produce on purpose.
 */

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "crc.h"
#include "custom_assert.h"
#include "high_score.h"
#include "mock_flash_bsp.h"

/* ==========================================================================
 * fixtures
 * ========================================================================= */

#define TEST_ERASED_BYTE (0xFFU)

/* The stored record's shape, written out here rather than shared with the module. A test
 * that reuses the module's own struct and its own CRC call would agree with a wrong
 * implementation; this one has to be told the layout, so a change to it shows up as a
 * failure rather than as silence. */
#define TEST_MAGIC_WORD  (0x5041434DUL)
/*! \brief The layout version the module writes today.
 *
 * Bumped every time the shape changes, and it has changed twice in two days: three tables became four
 * in DEC-055 and four became **two** in DEC-056, when the machine's tables went. The version is what
 * makes a page written before a change unreadable rather than silently misread, and a test that kept
 * an old number would be testing that the guard does *not* work. */
#define TEST_VERSION     (4U)
#define TEST_WORD_COUNT  (3U + (HIGH_SCORE_TABLE_COUNT * HIGH_SCORE_COUNT)) /* crc, magic, version, the tables */

/* The table the tests about *one* table use. Which one does not matter to them — what matters is
 * that they name it, so the tests about several tables read as being about something else. */
#define TEST_TABLE       (0U)

/* A second one, for the tests that are about the tables being separate. */
#define OTHER_TABLE      (1U)

static uint8_t g_page[FLASH_BSP_BLOCK_SIZE];
static uint8_t g_written_page[FLASH_BSP_BLOCK_SIZE];
static bool g_was_written;
static bool g_was_erased;

static void prv_on_read(uint8_t* out_block, size_t in_block_size, int in_call_count)
{
    (void)in_call_count;

    (void)memcpy(out_block, g_page, in_block_size);
}

static bool prv_on_replace(const uint8_t* in_block, size_t in_block_size, int in_call_count)
{
    (void)in_call_count;

    (void)memcpy(g_written_page, in_block, in_block_size);
    (void)memcpy(g_page, in_block, in_block_size);
    g_was_written = true;

    return true;
}

static bool prv_on_erase(int in_call_count)
{
    (void)in_call_count;

    (void)memset(g_page, TEST_ERASED_BYTE, sizeof(g_page));
    g_was_erased = true;

    return true;
}

/* Lay a valid table into the page the module will read, CRC and all. */
static void prv_write_page(uint32_t in_magic, uint32_t in_version, uint32_t in_first, uint32_t in_second,
                           uint32_t in_third, bool in_is_crc_valid)
{
    uint32_t words[TEST_WORD_COUNT] = {0};

    words[1] = in_magic;
    words[2] = in_version;
    words[3] = in_first;
    words[4] = in_second;
    words[5] = in_third;

    /* The CRC covers everything after itself. */
    words[0] = crc_32((const uint8_t*)&words[1], sizeof(words) - sizeof(words[0]));

    if (!in_is_crc_valid)
    {
        ++words[0];
    }

    (void)memset(g_page, 0, sizeof(g_page));
    (void)memcpy(g_page, words, sizeof(words));
}

static void prv_erase_page(void)
{
    (void)memset(g_page, TEST_ERASED_BYTE, sizeof(g_page));
}

void setUp(void)
{
    prv_erase_page();
    (void)memset(g_written_page, 0, sizeof(g_written_page));
    g_was_written = false;
    g_was_erased = false;

    /* Installed for every test, not only the one that expects an assertion: with no handler
     * registered, `custom_assert` spins in `while (1)`, and a hung executable says far less
     * than a named expression does. */
    assert_probe_begin();

    flash_bsp_read_Stub(prv_on_read);
    flash_bsp_replace_Stub(prv_on_replace);
    flash_bsp_erase_Stub(prv_on_erase);
}

void tearDown(void)
{
    assert_probe_end();
}

/* ==========================================================================
 * tests
 * ========================================================================= */

void test_an_erased_page_reads_as_an_empty_table(void)
{
    high_score_init();

    for (uint8_t index = 0U; index < HIGH_SCORE_COUNT; ++index)
    {
        TEST_ASSERT_EQUAL_UINT32(0U, high_score_get(TEST_TABLE, index));
    }

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));
}

void test_a_stored_table_comes_back(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(3000U, high_score_get(TEST_TABLE, 0U));
    TEST_ASSERT_EQUAL_UINT32(2000U, high_score_get(TEST_TABLE, 1U));
    TEST_ASSERT_EQUAL_UINT32(1000U, high_score_get(TEST_TABLE, 2U));
}

/* Three ways stored bytes lie, and the same answer to all of them: an empty table beats a
 * score nobody scored. */
void test_a_table_that_is_not_ours_is_discarded(void)
{
    prv_write_page(TEST_MAGIC_WORD + 1U, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));
}

void test_a_table_of_an_older_shape_is_discarded(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION + 1U, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));
}

void test_a_table_that_does_not_check_out_is_discarded(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, false);

    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));
}

void test_a_score_takes_its_place_and_pushes_the_rest_down(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_TRUE(high_score_offer(TEST_TABLE, 2500U));

    TEST_ASSERT_EQUAL_UINT32(3000U, high_score_get(TEST_TABLE, 0U));
    TEST_ASSERT_EQUAL_UINT32(2500U, high_score_get(TEST_TABLE, 1U));
    TEST_ASSERT_EQUAL_UINT32(2000U, high_score_get(TEST_TABLE, 2U));
}

void test_a_score_that_beats_nothing_changes_nothing(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();
    g_was_written = false;

    TEST_ASSERT_FALSE(high_score_offer(TEST_TABLE, 500U));
    TEST_ASSERT_FALSE(g_was_written);
    TEST_ASSERT_EQUAL_UINT32(1000U, high_score_get(TEST_TABLE, 2U));
}

/* First to get there keeps the place, the way the cabinets behaved. */
void test_an_equal_score_does_not_displace_the_one_already_there(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_FALSE(high_score_offer(TEST_TABLE, 1000U));
    TEST_ASSERT_EQUAL_UINT32(1000U, high_score_get(TEST_TABLE, 2U));
}

/* Zero is how an empty place is written down, so storing it would make an empty table
 * indistinguishable from three players who scored nothing. */
void test_a_score_of_zero_is_never_stored(void)
{
    high_score_init();

    TEST_ASSERT_FALSE(high_score_offer(TEST_TABLE, 0U));
    TEST_ASSERT_FALSE(g_was_written);
    TEST_ASSERT_FALSE(high_score_would_qualify(TEST_TABLE, 0U));
}

void test_a_new_score_survives_a_power_cycle(void)
{
    high_score_init();

    TEST_ASSERT_TRUE(high_score_offer(TEST_TABLE, 1234U));

    /* The stub keeps what was written, so re-initialising is the power cycle. */
    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(1234U, high_score_get_best(TEST_TABLE));
}

void test_qualifying_is_answered_without_changing_anything(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();
    g_was_written = false;

    TEST_ASSERT_TRUE(high_score_would_qualify(TEST_TABLE, 1001U));
    TEST_ASSERT_FALSE(high_score_would_qualify(TEST_TABLE, 1000U));
    TEST_ASSERT_FALSE(g_was_written);
    TEST_ASSERT_EQUAL_UINT32(1000U, high_score_get(TEST_TABLE, 2U));
}

/* Erased rather than zeroed, so afterwards storage is in the state the very first boot
 * sees — which means the empty case can be exercised on demand instead of once ever. */
void test_a_reset_erases_the_page_and_the_table(void)
{
    prv_write_page(TEST_MAGIC_WORD, TEST_VERSION, 3000U, 2000U, 1000U, true);

    high_score_init();

    TEST_ASSERT_TRUE(high_score_reset());
    TEST_ASSERT_TRUE(g_was_erased);
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));

    high_score_init();

    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(TEST_TABLE));
}

/* --- three tables, one page (FR-041) -------------------------------------- */

/* The whole point of three tables: a run of one game leaves the other two alone. Without this the
 * feature would be a table with three names for it. */
void test_the_tables_do_not_see_each_other(void)
{
    high_score_init();

    TEST_ASSERT_TRUE(high_score_offer(TEST_TABLE, 5000U));

    TEST_ASSERT_EQUAL_UINT32(5000U, high_score_get_best(TEST_TABLE));
    TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(OTHER_TABLE));

    /* And a score that would not get into the first table still gets into the empty second one —
     * which a shared table, or a shared "would qualify", would refuse. */
    TEST_ASSERT_TRUE(high_score_would_qualify(OTHER_TABLE, 100U));
    TEST_ASSERT_TRUE(high_score_offer(OTHER_TABLE, 100U));

    TEST_ASSERT_EQUAL_UINT32(5000U, high_score_get_best(TEST_TABLE));
    TEST_ASSERT_EQUAL_UINT32(100U, high_score_get_best(OTHER_TABLE));
}

void test_every_table_survives_a_power_cycle(void)
{
    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        (void)high_score_offer(table, (uint32_t)(1000U * (table + 1U)));
    }

    /* The page holds all three, so one write per offer must not lose the ones written before it. */
    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(1000U * (table + 1U)), high_score_get_best(table));
    }
}

void test_a_reset_clears_every_table(void)
{
    high_score_init();

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        (void)high_score_offer(table, 4200U);
    }

    TEST_ASSERT_TRUE(high_score_reset());

    for (uint8_t table = 0U; table < HIGH_SCORE_TABLE_COUNT; ++table)
    {
        TEST_ASSERT_EQUAL_UINT32(0U, high_score_get_best(table));
    }
}

/* --- preconditions -------------------------------------------------------- */

void test_asking_for_a_place_that_does_not_exist_trips_the_assert(void)
{
    high_score_init();

    ASSERT_PROBE_EXPECT(high_score_get(TEST_TABLE, HIGH_SCORE_COUNT), "in_index < HIGH_SCORE_COUNT");
}

void test_asking_for_a_table_that_does_not_exist_trips_the_assert(void)
{
    high_score_init();

    ASSERT_PROBE_EXPECT(high_score_get(HIGH_SCORE_TABLE_COUNT, 0U), "in_table < HIGH_SCORE_TABLE_COUNT");
}
