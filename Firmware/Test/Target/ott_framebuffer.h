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
