/*
 * render.h
 *
 * Draws a display list and gets it onto the panel — the last step before pixels.
 *
 * It owns the one frame buffer, and it is the only module that decides **what to
 * transfer**. That is the expensive decision: two small rectangles cost 2.08 ms and a
 * whole frame costs 252 ms ([M2 Board Bring-Up §3](../../../Docu/Design/M2-Board-Bring-Up.md)),
 * and nothing above here knows those numbers.
 *
 * What it does *not* know is just as deliberate: not the maze, not the tile size, not
 * where the score goes. A display list is "this drawing, that palette, here", and every
 * layout question was settled in `game_view` where a unit test can reach it.
 *
 * **Erasing is done by save-under.** Before an actor is drawn, the pixels it is about to
 * cover are copied out; next frame they are put back, and the actor is drawn at its new
 * place. The obvious alternative — keeping a clean copy of the background — needs a
 * second frame buffer, and two of those are 300 kB against 256 kB of RAM. Five actors of
 * 20 x 20 are 4 kB.
 *
 * The one thing save-under cannot know is that the background itself changed: restoring
 * a swallowed pellet would put the dot back. That is what `DISPLAY_ITEM_BACKGROUND` is
 * for, and why the order inside #render_draw matters.
 */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "framebuffer.h"
#include "msg.h"

/* ==========================================================================
 * render - public types
 * ========================================================================= */

/*! \brief Largest sprite this module can save the pixels under, per side. The sprite set
 *         is built to the tile size; anything bigger would need more save-under memory
 *         than has been set aside, so it is asserted rather than silently clipped. */
#define RENDER_SPRITE_MAX_SIZE (20U)

/* ==========================================================================
 * render - public API
 * ========================================================================= */

/*! \brief Bring the display up and blank the frame buffer. */
void render_init(void);

/*! \brief Draw a display list and transfer what changed.
 *
 * Order matters and is the whole design:
 *
 *  1. put back the pixels under every actor drawn last time,
 *  2. apply the background items — so a cell that changed wins over what was restored,
 *  3. save the pixels under each actor's new position, then draw it,
 *  4. transfer, per actor, the rectangle spanning where it was and where it is.
 *
 * \param[in]       in_list: what to draw, must not be `NULL`
 */
void render_draw(const msg_display_list_t* in_list);

/*! \brief The frame buffer, for a host application that has to blit it into a window.
 *
 * \return          The buffer, never `NULL` and owned by this module
 */
const framebuffer_t* render_get_framebuffer(void);

#endif /* RENDER_H */
