/*
 * Host implementation of the display port.
 *
 * Headless: it keeps the last presented frame and a few counters, which is what a
 * viewer or a test needs. The SDL window required by CON-103 / FR-104 is a consumer of
 * this, not a replacement for it — it reads display_host_get_last_frame() and blits it.
 * Keeping the port itself headless means host tests never need a window or a display
 * server.
 */
#include "display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "display_host.h"
#include "framebuffer.h"

/* ==========================================================================
 * display_host - private
 * ========================================================================= */

static framebuffer_t g_last_frame;
static uint32_t g_present_count;
static uint32_t g_service_count;
static bool g_is_enabled;

/* ==========================================================================
 * display - public (the port)
 * ========================================================================= */

void display_init(void)
{
    g_present_count = 0U;
    g_service_count = 0U;
    g_is_enabled = true;

    display_clear();
}

void display_present(const framebuffer_t* in_framebuffer)
{
    ASSERT(in_framebuffer != NULL);

    memcpy(&g_last_frame, in_framebuffer, sizeof(g_last_frame));

    ++g_present_count;
}

void display_clear(void)
{
    framebuffer_clear(&g_last_frame);
}

void display_set_enabled(bool in_is_enabled)
{
    g_is_enabled = in_is_enabled;
}

void display_service(void)
{
    /* No COM inversion to keep alive off-target; counted so a test can prove the
     * caller is servicing the display at all. */
    ++g_service_count;
}

/* ==========================================================================
 * display_host - public (host-only extras)
 * ========================================================================= */

const framebuffer_t* display_host_get_last_frame(void)
{
    return &g_last_frame;
}

uint32_t display_host_get_present_count(void)
{
    return g_present_count;
}

uint32_t display_host_get_service_count(void)
{
    return g_service_count;
}

bool display_host_is_enabled(void)
{
    return g_is_enabled;
}
