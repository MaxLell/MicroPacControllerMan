#include "retain_ram.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"

#if !defined(__GNUC__)
#error "This code relies on GNU extensions. Please compile with a GNU compatible compiler."
#endif /* !defined(__GNUC__) */

/* ==========================================================================
 * retain_ram - private
 * ========================================================================= */

#if defined(PACMAN_HOST_BUILD) || defined(TEST)
/* A host process has no `.noinit` section, and nothing survives its restart
 * anyway. A plain buffer keeps the module linkable and unit-testable; the
 * survive-a-reset property is inherently target-only. */
static uint8_t g_retained_memory_buffer[RETAIN_RAM_BUFFER_SIZE];
#else  /* defined(PACMAN_HOST_BUILD) || defined(TEST) */
/* The `.noinit` section is not touched by the startup code, so the contents
 * survive a software reset. The section is declared in the linker script and is
 * marked NON-GENERATED there — it has to be re-added after a CubeMX
 * re-generation (RF-005). */
__attribute__((section(".noinit"), used, aligned(4))) static uint8_t g_retained_memory_buffer[RETAIN_RAM_BUFFER_SIZE];
#endif /* !defined(PACMAN_HOST_BUILD) && !defined(TEST) */

/* ==========================================================================
 * retain_ram - public
 * ========================================================================= */

void retained_ram_write(const uint8_t* const in_buffer, const size_t in_buffer_size)
{
    ASSERT(in_buffer != NULL);
    ASSERT(in_buffer_size == RETAIN_RAM_BUFFER_SIZE);

    memcpy(g_retained_memory_buffer, in_buffer, in_buffer_size);
}

void retained_ram_read(uint8_t* const out_buffer, const size_t in_buffer_size)
{
    ASSERT(out_buffer != NULL);
    ASSERT(in_buffer_size == RETAIN_RAM_BUFFER_SIZE);

    memcpy(out_buffer, g_retained_memory_buffer, in_buffer_size);
}
