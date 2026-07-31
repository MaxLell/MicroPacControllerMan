/*
 * game_view.h
 *
 * Turns a game state into what should be on the screen: cells become pixels, a step in
 * progress becomes a position between two cells, and each actor picks its drawing and
 * its colours.
 *
 * **This module owns the screen layout.** The tile size, where the maze sits on the
 * panel, which sprite belongs to a frightened ghost — all of it is here, and none of it
 * is anywhere below. That is what leaves Render with nothing to know except how to draw
 * a sprite at a pixel position, and it is what lets every layout decision be checked by
 * a unit test instead of by looking at a panel.
 *
 * **It runs at the frame rate, not the game's step rate.** Between two simulation steps
 * it is asked nine or ten times and answers with the actors a little further along each
 * time ([10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)). Feeding it a
 * state and then asking once is not an error, it just produces one still picture.
 */

#ifndef GAME_VIEW_H
#define GAME_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * game_view - public types
 * ========================================================================= */

/*! \brief Side of one maze cell on the panel, in pixels.
 *
 * 20 falls out of the panel rather than out of taste: 11 cells across the 240 px width
 * is 220, leaving a 10 px margin either side, and 20 is even — so a cell has a real
 * centre, which every interpolation and every sprite placement here depends on. */
#define GAME_VIEW_TILE_SIZE (20)

/*! \brief Where the maze's top-left corner sits on the panel. Horizontally centred; the
 *         180 px of maze start near the top so the space below stays free for the score
 *         and lives that FR-002 and FR-006 will need. */
#define GAME_VIEW_ORIGIN_X  ((FRAMEBUFFER_WIDTH - (PLAYFIELD_WIDTH * GAME_VIEW_TILE_SIZE)) / 2)
#define GAME_VIEW_ORIGIN_Y  (10)

typedef struct
{
    /*!< The maze of the current level. 516 bytes, loaded once when the level changes
     *   rather than per cell — walls do not travel in the state, because they never
     *   change during a level and 99 unchanging bits per frame would be waste. */
    playfield_t maze;

    /*!< The last state received. Held rather than consumed, because the view is asked
     *   for a picture far more often than the game produces a state. */
    msg_game_state_t state;
    bool has_state;

    /*!< What the field looked like when it was last described, so a change can be told
     *   from a repetition. A pellet vanishing is a change to the *field*, and Render
     *   cannot infer it — an actor sprite moving off that cell would otherwise put the
     *   pellet back. */
    msg_game_state_t drawn_field;
    bool has_drawn_field;

    /*!< Set while a whole field is being handed over — a level change, or the first
     *   state after a reset. That takes many messages, so the walk keeps its place. A
     *   *changed* cell during play is different: it is rare, it fits beside the actors,
     *   and it goes out in the same frame. */
    uint8_t pending_field_cell;
    bool is_full_field_pending;
} game_view_t;

/* ==========================================================================
 * game_view - public API
 * ========================================================================= */

/*! \brief Reset the view. Nothing is drawn and the next state is treated as a fresh
 *         start, so the whole field is described again.
 *
 * \param[out]      inout_view: instance to initialise, must not be `NULL`
 */
void game_view_init(game_view_t* inout_view);

/*! \brief Take a new game state.
 *
 * \param[in,out]   inout_view: the view, must not be `NULL`
 * \param[in]       in_state: the state, copied in; must not be `NULL`
 */
void game_view_set_state(game_view_t* inout_view, const msg_game_state_t* in_state);

/*! \brief Fill in the next display list.
 *
 * Call it once per frame. A frame is normally the five actors; when the field has
 * changed — a new level, or a pellet eaten — the field goes out first, over as many
 * calls as it takes, and #game_view_is_field_pending says whether more is waiting.
 *
 * \param[in,out]   inout_view: the view, must not be `NULL`
 * \param[out]      out_list: filled with what to draw, must not be `NULL`
 * \return          `true` when there is anything to draw at all
 */
bool game_view_get_display_list(game_view_t* inout_view, msg_display_list_t* out_list);

/*! \brief Whether a field handover is still in progress, so the caller keeps asking
 *         before it lets the frame go.
 *
 * \param[in]       in_view: the view, must not be `NULL`
 * \return          `true` while field tiles remain undelivered
 */
bool game_view_is_field_pending(const game_view_t* in_view);

/*! \brief Where a cell's top-left pixel is, for a caller that needs the same arithmetic
 *         — a test, or a HUD placed relative to the maze.
 *
 * \param[in]       in_column: cell column
 * \param[in]       in_row: cell row
 * \param[out]      out_x: left edge in pixels, must not be `NULL`
 * \param[out]      out_y: top edge in pixels, must not be `NULL`
 */
void game_view_get_cell_pixel(uint8_t in_column, uint8_t in_row, int16_t* out_x, int16_t* out_y);

#endif /* GAME_VIEW_H */
