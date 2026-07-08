#include "retain_ram.h"

/* Placed in `.noinit` (see linker script) so it is not zeroed at startup and
 * therefore survives a software reset. */
static ott_spec_t g_ott_spec __attribute__((section(".noinit"), used));

ott_spec_t* retain_ott_spec(void)
{
    return &g_ott_spec;
}
