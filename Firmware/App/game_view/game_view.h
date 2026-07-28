/*
 * game_view.h
 *
 * Draws a #game_snapshot_t into a frame buffer — the View of the MVP split
 * ([03 §3.1](../../../Docu/PrePlanning/03-Architecture.md)), and the single rendering
 * output of §3.6.
 *
 * Platform-independent on purpose: it uses only `Services/gfx` and `Services/framebuffer`,
 * so the same code produces the picture on the host and on the panel. That is the point —
 * what you see in the SDL window is not an approximation of the device, it is the identical
 * frame buffer the display driver would clock out.
 *
 * Reads the snapshot and writes the buffer, and holds nothing between calls.
 */

#ifndef GAME_VIEW_H
#define GAME_VIEW_H

#include "framebuffer.h"
#include "game.h"

/* ==========================================================================
 * game_view - public API
 * ========================================================================= */

/*! \brief Draw a frame.
 *
 * Clears the buffer and renders the maze, the pellets, Pacman, the ghosts and the HUD. The
 * caller decides when to show the result (`display_present`).
 *
 * \param[out]      inout_framebuffer: buffer to draw into, must not be `NULL`
 * \param[in]       in_snapshot: what to draw, must not be `NULL`
 */
void game_view_draw(framebuffer_t* inout_framebuffer, const game_snapshot_t* in_snapshot);

#endif /* GAME_VIEW_H */
