/*
 * Unit tests for Services/crc.
 *
 * CRC-32/ISO-HDLC has published check values, so these tests compare against the
 * standard rather than against this implementation's own output — an
 * implementation that is self-consistently wrong would still pass the latter.
 */
#include <stdint.h>
#include <string.h>

/* Not used directly — Ceedling picks the sources to link from the includes it sees
 * here, and every test executable links Test/support/assert_probe.c, which needs
 * custom_assert. Without this the link fails. */
#include "assert_probe.h"
#include "crc.h"
#include "custom_assert.h"
#include "unity.h"

/* The canonical check value of CRC-32/ISO-HDLC: the ASCII string "123456789". */
#define TEST_CHECK_INPUT "123456789"
#define TEST_CHECK_VALUE (0xCBF43926U)

/* Further published vectors for the same variant. */
#define TEST_SINGLE_A_VALUE (0xE8B7BE43U)  /* "a"                 */
#define TEST_ABC_VALUE (0x352441C2U)       /* "abc"               */
#define TEST_ZERO_BYTE_VALUE (0xD202EF8DU) /* one 0x00 byte       */

void setUp(void)
{
    assert_probe_begin();
}

void tearDown(void)
{
    assert_probe_end();
}

static uint32_t prv_crc_of_string(const char* in_text)
{
    return crc_32((const uint8_t*)in_text, strlen(in_text));
}

void test_crc_32_matches_the_canonical_check_value(void)
{
    TEST_ASSERT_EQUAL_HEX32(TEST_CHECK_VALUE, prv_crc_of_string(TEST_CHECK_INPUT));
}

void test_crc_32_matches_further_published_vectors(void)
{
    const uint8_t zero_byte = 0x00U;

    TEST_ASSERT_EQUAL_HEX32(TEST_SINGLE_A_VALUE, prv_crc_of_string("a"));
    TEST_ASSERT_EQUAL_HEX32(TEST_ABC_VALUE, prv_crc_of_string("abc"));
    TEST_ASSERT_EQUAL_HEX32(TEST_ZERO_BYTE_VALUE, crc_32(&zero_byte, sizeof(zero_byte)));
}

void test_crc_32_of_an_empty_range_is_zero(void)
{
    const uint8_t data = 0xA5U;

    TEST_ASSERT_EQUAL_HEX32(0U, crc_32(&data, 0U));
}

void test_crc_32_accepts_null_for_an_empty_range(void)
{
    TEST_ASSERT_EQUAL_HEX32(0U, crc_32(NULL, 0U));
}

void test_crc_32_asserts_on_null_with_a_non_empty_range(void)
{
    ASSERT_PROBE_EXPECT(crc_32(NULL, 1U), "(in_data != NULL) || (in_size == 0U)");
}

/* A single flipped bit anywhere must change the checksum — that is the whole point
 * of using this instead of a sum. */
void test_crc_32_detects_a_single_flipped_bit(void)
{
    uint8_t data[] = {0xDEU, 0xADU, 0xBEU, 0xEFU, 0x00U, 0x11U, 0x22U, 0x33U};
    const uint32_t reference = crc_32(data, sizeof(data));

    for (size_t index = 0U; index < sizeof(data); ++index)
    {
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            data[index] ^= (uint8_t)(1U << bit);
            TEST_ASSERT_NOT_EQUAL_HEX32(reference, crc_32(data, sizeof(data)));
            data[index] ^= (uint8_t)(1U << bit);
        }
    }
}

/* Trailing zero bytes must not be invisible: a plain additive checksum cannot tell
 * these two apart, and the OTT request is a fixed-size buffer that is mostly zero. */
void test_crc_32_distinguishes_trailing_zero_bytes(void)
{
    const uint8_t shorter[] = {0x01U, 0x02U};
    const uint8_t longer[] = {0x01U, 0x02U, 0x00U};

    TEST_ASSERT_NOT_EQUAL_HEX32(crc_32(shorter, sizeof(shorter)), crc_32(longer, sizeof(longer)));
}

/* Order matters — another property a sum does not have. */
void test_crc_32_is_order_sensitive(void)
{
    const uint8_t forward[] = {0x01U, 0x02U, 0x03U};
    const uint8_t reversed[] = {0x03U, 0x02U, 0x01U};

    TEST_ASSERT_NOT_EQUAL_HEX32(crc_32(forward, sizeof(forward)),
                                crc_32(reversed, sizeof(reversed)));
}
