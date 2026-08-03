/*
 * ott_framebuffer.h
 *
 * The one frame buffer the on-target tests draw into.
 *
 * A frame is 153,600 bytes, 60 % of the contiguous SRAM, so a second one does not
 * fit — a scenario cannot simply declare its own. It does not need to either: the
 * OTT flow runs exactly one scenario per reset ([09 OTT
 * Mechanism](../../../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)), so the
 * buffer is handed out rather than owned by whichever test asked first.
 *
 * **This module used to own that buffer, and `App/render` grew its own on exactly the
 * same reasoning** — two modules each certain it held the only one. Together they are
 * 300 kB against 256 kB, which nothing noticed while the game was not yet in the target
 * image. Render owns it now and this hands it on, so the arithmetic works out at one.
 *
 * The dependency runs the wrong way for a bring-up test — `Test/Target` on `App/` — and
 * that is deliberate rather than overlooked: the alternative was to lift the buffer into
 * a module of its own, and one forwarding call was judged the smaller price. What the
 * tests must *not* do is draw through Render: they exercise the `gfx` -> `display` path
 * below it, and `ott_display_test` measures exactly the transfer decision that Render
 * would take over.
 */

#ifndef OTT_FRAMEBUFFER_H
#define OTT_FRAMEBUFFER_H

#include "framebuffer.h"

/* ==========================================================================
 * ott_framebuffer - public API
 * ========================================================================= */

/*! \brief Borrow the shared frame buffer.
 *
 * \return          The buffer, never `NULL`. Its contents are whatever the caller
 *                      left there, so a scenario clears or fills it first.
 */
framebuffer_t* ott_framebuffer_get(void);

#endif /* OTT_FRAMEBUFFER_H */
