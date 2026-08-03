#include "high_score.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "crc.h"
#include "custom_assert.h"
#include "flash_bsp.h"

/* ==========================================================================
 * high_score - private
 * ========================================================================= */

/* Arbitrary, and only has to be unlikely: it is what tells a high-score table from an
 * erased page, from another build's leftovers, and from whatever a half-finished write
 * left behind. */
#define MAGIC_WORD     (0x5041434DUL) /* "PACM" */

/* Bumped whenever the stored shape changes, so an old table is discarded rather than
 * misread. A CRC would not catch this on its own — a table of the previous shape is
 * perfectly intact, just not this shape. */
#define LAYOUT_VERSION (1U)

/* The record as it sits in flash. The CRC is deliberately first and covers everything
 * after it, so the field being checked never covers itself. */
typedef struct
{
    uint32_t crc;
    uint32_t magic_word;
    uint32_t layout_version;
    uint32_t scores[HIGH_SCORE_COUNT];
} stored_table_t;

static uint32_t g_scores[HIGH_SCORE_COUNT];

static uint32_t prv_compute_crc(const stored_table_t* const in_table)
{
    const uint8_t* const checked = (const uint8_t*)&in_table->magic_word;
    const size_t checked_size = sizeof(*in_table) - offsetof(stored_table_t, magic_word);

    return crc_32(checked, checked_size);
}

static void prv_clear(void)
{
    (void)memset(g_scores, 0, sizeof(g_scores));
}

/* Write the table out. The block is zeroed first so the bytes past the record are a known
 * value rather than whatever the stack held — a stored page is a thing someone will read in
 * a debugger one day. */
static bool prv_store(void)
{
    uint8_t block[FLASH_BSP_BLOCK_SIZE] = {0};
    stored_table_t table = {0};

    table.magic_word = MAGIC_WORD;
    table.layout_version = LAYOUT_VERSION;
    (void)memcpy(table.scores, g_scores, sizeof(table.scores));
    table.crc = prv_compute_crc(&table);

    (void)memcpy(block, &table, sizeof(table));

    return flash_bsp_replace(block, sizeof(block));
}

/* ==========================================================================
 * high_score - public
 * ========================================================================= */

void high_score_init(void)
{
    uint8_t block[FLASH_BSP_BLOCK_SIZE];
    stored_table_t table;

    /* A build mistake, not a runtime condition: the record has to fit the block it is
     * stored in. */
    ASSERT(sizeof(table) <= sizeof(block));

    prv_clear();

    flash_bsp_read(block, sizeof(block));
    (void)memcpy(&table, block, sizeof(table));

    if ((table.magic_word != MAGIC_WORD) || (table.layout_version != LAYOUT_VERSION))
    {
        return;
    }

    if (table.crc != prv_compute_crc(&table))
    {
        return;
    }

    (void)memcpy(g_scores, table.scores, sizeof(g_scores));
}

bool high_score_offer(uint32_t in_score)
{
    uint8_t place;

    if (in_score == 0U)
    {
        return false;
    }

    for (place = 0U; place < HIGH_SCORE_COUNT; ++place)
    {
        if (in_score > g_scores[place])
        {
            break;
        }
    }

    if (place >= HIGH_SCORE_COUNT)
    {
        return false;
    }

    /* Everything below the new score moves down a place, and the last one falls off. */
    for (uint8_t index = HIGH_SCORE_COUNT - 1U; index > place; --index)
    {
        g_scores[index] = g_scores[index - 1U];
    }

    g_scores[place] = in_score;

    (void)prv_store();

    return true;
}

uint32_t high_score_get(uint8_t in_index)
{
    ASSERT(in_index < HIGH_SCORE_COUNT);

    return g_scores[in_index];
}

uint32_t high_score_get_best(void)
{
    return g_scores[0];
}

bool high_score_would_qualify(uint32_t in_score)
{
    return (in_score > 0U) && (in_score > g_scores[HIGH_SCORE_COUNT - 1U]);
}

bool high_score_reset(void)
{
    prv_clear();

    return flash_bsp_erase();
}
