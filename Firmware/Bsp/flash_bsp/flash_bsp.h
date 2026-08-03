/*
 * flash_bsp.h
 *
 * The one page of non-volatile memory the application is allowed to write, behind an
 * interface that says nothing about flash: read a block, replace a block. Backs the high
 * score of FR-009, which has to survive a power cycle and so cannot live in
 * `Bsp/retain_ram`.
 *
 * **The page is reserved by the linker, not by convention.** `FLASH` is one 8 KB page
 * shorter than the part and the page beyond it is its own region, so a firmware that grew
 * into it would fail to link rather than erase part of itself the first time a score was
 * saved. See `ThirdParty/STM32_U545RE_HAL/STM32U545xx_FLASH.ld`.
 *
 * **Replace, not write.** Flash erases a whole page and can only clear bits, so a partial
 * write is not a thing this hardware does. Offering `write(offset, bytes)` would be a
 * promise the driver cannot keep, and the caller would find out the first time it wrote
 * twice. #flash_bsp_replace takes the whole block, erases, and programs — one call, one
 * consistent page, nothing half-written unless the power goes at exactly the wrong
 * moment, which is what the caller's checksum is for.
 *
 * A platform port: `flash_bsp.c` on the target, `flash_bsp_host.c` against a file, so the
 * host application remembers scores between runs and the store above it is testable.
 */

#ifndef FLASH_BSP_H
#define FLASH_BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * flash_bsp - public API
 * ========================================================================= */

/*! \brief Largest block the page holds, in bytes.
 *
 * Far smaller than the 8 KB page: what goes in it is a handful of scores, and a block that
 * is a whole page would make every caller carry 8 KB of RAM to write one. The target
 * programs 128 bits at a time, so this is a multiple of 16.
 */
#define FLASH_BSP_BLOCK_SIZE (256U)

/*! \brief Prepare the driver. Call once before anything else. */
void flash_bsp_init(void);

/*! \brief Read the stored block.
 *
 * Always succeeds — an erased page reads back as all bits set, which is a perfectly good
 * answer meaning "nothing has ever been stored here". Recognising that is the caller's
 * business, because only the caller knows what a valid block looks like.
 *
 * \param[out]      out_block: receives #FLASH_BSP_BLOCK_SIZE bytes, must not be `NULL`
 * \param[in]       in_block_size: must equal #FLASH_BSP_BLOCK_SIZE
 */
void flash_bsp_read(uint8_t* out_block, size_t in_block_size);

/*! \brief Erase the page and program a new block into it.
 *
 * Takes a few milliseconds and stalls the CPU while the flash controller works — an
 * erase on this part is not a background operation. Call it when a score changes, not
 * once a frame.
 *
 * \param[in]       in_block: #FLASH_BSP_BLOCK_SIZE bytes to store, must not be `NULL`
 * \param[in]       in_block_size: must equal #FLASH_BSP_BLOCK_SIZE
 * \return          `true` when the page now holds exactly these bytes
 */
bool flash_bsp_replace(const uint8_t* in_block, size_t in_block_size);

/*! \brief Erase the page, leaving it as it left the factory.
 *
 * What a "reset the high scores" command does. Separate from writing a zeroed block,
 * because an erased page is how the store recognises that nothing was ever saved — and
 * that is exactly the state being asked for.
 *
 * \return          `true` when the page is erased
 */
bool flash_bsp_erase(void);

#endif /* FLASH_BSP_H */
