/*
 * flash_bsp_host.c
 *
 * The host half of the flash port: one small file next to the executable.
 *
 * A file rather than a static array, because the property being stood in for is *surviving
 * a power cycle*, and an array in a process that exits stands in for nothing. So the SDL
 * application remembers scores between runs, which is the only way to play the high-score
 * table before there is a board to play it on.
 *
 * The erased state is modelled honestly too: a missing file reads back as all bits set,
 * exactly as an erased page does, so the store above cannot pass here by treating "no
 * file" as a special case that the target never produces.
 */

#include "flash_bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "custom_assert.h"

/* ==========================================================================
 * flash_bsp_host - private
 * ========================================================================= */

#define FILE_NAME   "pacman_nvm.bin"

#define ERASED_BYTE (0xFFU)

/* ==========================================================================
 * flash_bsp_host - public
 * ========================================================================= */

void flash_bsp_init(void)
{
}

void flash_bsp_read(uint8_t* out_block, size_t in_block_size)
{
    FILE* file;

    ASSERT(out_block != NULL);
    ASSERT(in_block_size == FLASH_BSP_BLOCK_SIZE);

    (void)memset(out_block, ERASED_BYTE, in_block_size);

    file = fopen(FILE_NAME, "rb");

    if (file == NULL)
    {
        /* Never written: an erased page, which is what the caller has to cope with anyway. */
        return;
    }

    /* A short read leaves the rest erased, which is the same thing a half-programmed page
     * looks like — and the caller's checksum is what rejects both.
     *
     * Taken into a variable rather than cast to `(void)`: glibc marks `fread` warn_unused_result,
     * and a cast does not satisfy that. Only visible at -O2, which the training library uses. */
    const size_t read_bytes = fread(out_block, 1U, in_block_size, file);
    (void)read_bytes;
    (void)fclose(file);
}

bool flash_bsp_replace(const uint8_t* in_block, size_t in_block_size)
{
    FILE* file;
    size_t written;

    ASSERT(in_block != NULL);
    ASSERT(in_block_size == FLASH_BSP_BLOCK_SIZE);

    file = fopen(FILE_NAME, "wb");

    if (file == NULL)
    {
        return false;
    }

    written = fwrite(in_block, 1U, in_block_size, file);

    return (fclose(file) == 0) && (written == in_block_size);
}

bool flash_bsp_erase(void)
{
    (void)remove(FILE_NAME);

    return true;
}
