#include "ott_framebuffer.h"

#include "framebuffer.h"

/* ==========================================================================
 * ott_framebuffer - private
 * ========================================================================= */

/* 153,600 bytes. Static, because it fits neither on a stack nor twice in SRAM. */
static framebuffer_t g_framebuffer;

/* ==========================================================================
 * ott_framebuffer - public
 * ========================================================================= */

framebuffer_t* ott_framebuffer_get(void)
{
    return &g_framebuffer;
}
