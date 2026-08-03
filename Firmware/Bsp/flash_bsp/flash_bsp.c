#include "flash_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "main.h"

/* ==========================================================================
 * flash_bsp - private
 * ========================================================================= */

/* The page comes from the linker, so the address erased here and the address kept free
 * over there cannot drift apart — see the NVM region in the linker script. */
extern uint8_t __nvm_start__[];
extern uint8_t __nvm_size__[];

/* The controller programs 128 bits at a time and takes the *address* of the source, so a
 * block written from an unaligned buffer would fault. */
#define QUAD_WORD_SIZE (16U)

static uint32_t prv_get_page_address(void)
{
    return (uint32_t)(uintptr_t)__nvm_start__;
}

/* Which page of which bank the reserved region is. Derived rather than written down: the
 * region moves by editing one line of the linker script, and nothing here has to follow.
 */
static uint32_t prv_get_page_index(void)
{
    return ((prv_get_page_address() - FLASH_BASE) % FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
}

static uint32_t prv_get_bank(void)
{
    return ((prv_get_page_address() - FLASH_BASE) < FLASH_BANK_SIZE) ? FLASH_BANK_1 : FLASH_BANK_2;
}

/* The instruction cache sits in front of the flash and caches literal reads from it too, so
 * a page that has just been erased and reprogrammed can still be read back as whatever was
 * cached before. That is not a theory: the first run of `ott high_score` on the board wrote
 * three scores, reported success, and read back an empty table. Every operation here
 * therefore runs with the cache off and leaves it invalidated. */
static void prv_suspend_cache(void)
{
    (void)HAL_ICACHE_Disable();
}

static void prv_resume_cache(void)
{
    (void)HAL_ICACHE_Invalidate();
    (void)HAL_ICACHE_Enable();
}

static bool prv_erase_page(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t failed_page = 0U;
    bool is_erased;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = prv_get_bank();
    erase.Page = prv_get_page_index();
    erase.NbPages = 1U;

    is_erased = (HAL_FLASHEx_Erase(&erase, &failed_page) == HAL_OK);

    return is_erased;
}

/* ==========================================================================
 * flash_bsp - public
 * ========================================================================= */

void flash_bsp_init(void)
{
    /* The region has to hold a block and the block has to be programmable in whole
     * quad-words. Both are decided at build time, so this is a build mistake caught at
     * start-up rather than a runtime condition. */
    ASSERT((uint32_t)(uintptr_t)__nvm_size__ >= FLASH_BSP_BLOCK_SIZE);
    ASSERT((FLASH_BSP_BLOCK_SIZE % QUAD_WORD_SIZE) == 0U);
    ASSERT((prv_get_page_address() % FLASH_PAGE_SIZE) == 0U);
}

void flash_bsp_read(uint8_t* out_block, size_t in_block_size)
{
    ASSERT(out_block != NULL);
    ASSERT(in_block_size == FLASH_BSP_BLOCK_SIZE);

    /* Flash is memory-mapped for reading, so this is the whole of a read. */
    (void)memcpy(out_block, (const void*)(uintptr_t)prv_get_page_address(), in_block_size);
}

bool flash_bsp_replace(const uint8_t* in_block, size_t in_block_size)
{
    /* Aligned and static because HAL_FLASH_Program is handed the *address* of the source
     * and reads 128 bits from it; a caller's buffer need not be aligned that far. */
    static uint64_t g_staging[FLASH_BSP_BLOCK_SIZE / sizeof(uint64_t)] __attribute__((aligned(QUAD_WORD_SIZE)));
    bool is_written;

    ASSERT(in_block != NULL);
    ASSERT(in_block_size == FLASH_BSP_BLOCK_SIZE);

    (void)memcpy(g_staging, in_block, in_block_size);

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    prv_suspend_cache();

    is_written = prv_erase_page();

    for (size_t offset = 0U; is_written && (offset < in_block_size); offset += QUAD_WORD_SIZE)
    {
        const uint32_t address = prv_get_page_address() + (uint32_t)offset;
        const uint8_t* const source = &((const uint8_t*)g_staging)[offset];

        is_written = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, address, (uint32_t)(uintptr_t)source) == HAL_OK);
    }

    prv_resume_cache();

    (void)HAL_FLASH_Lock();

    return is_written;
}

bool flash_bsp_erase(void)
{
    bool is_erased;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    prv_suspend_cache();

    is_erased = prv_erase_page();

    prv_resume_cache();

    (void)HAL_FLASH_Lock();

    return is_erased;
}
