/*
 * playfield.h
 *
 * The maze: static walls plus which pellets are left. Answers "can an entity stand
 * here", removes eaten pellets, and reports when the maze is empty
 * ([10 §10.2](../../../Docu/PrePlanning/10-Pacman-Game-Design.md), FR-010/011/017/021/022/025).
 *
 * Pure logic, no hardware and no messages — a plain synchronous module the game owns.
 * Walkability is a *query*; routing it through a queue would make the movement code
 * unwritable, which is why this is not an Active Object.
 *
 * Coordinates are signed and may legitimately point outside the grid: a ghost's target
 * cell can be off-maze (§10.4's Pinky and Inky compute one), and Manhattan distance to
 * an off-maze target is still meaningful. Stepping is what wraps (#playfield_wrap_cell).
 */

#ifndef PLAYFIELD_H
#define PLAYFIELD_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"

/* ==========================================================================
 * playfield - public types
 * ========================================================================= */

#define PLAYFIELD_WIDTH          (28)
#define PLAYFIELD_HEIGHT         (31)

/*! \brief Cells in the ghost pen (§10.2). */
#define PLAYFIELD_PEN_CELL_COUNT (4)

typedef struct
{
    int16_t x; /*!< Column, `0` is the left edge      */
    int16_t y; /*!< Row, `0` is the top edge          */
} cell_t;

typedef enum
{
    PLAYFIELD_PELLET_NONE = 0, /*!< Nothing to eat here               */
    PLAYFIELD_PELLET_NORMAL,   /*!< 10 points (§10.6)                 */
    PLAYFIELD_PELLET_POWER     /*!< 50 points, triggers frightened     */
} playfield_pellet_e;

typedef struct
{
    bool walls[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    bool tunnels[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    playfield_pellet_e pellets[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    uint16_t remaining_pellet_count; /*!< Normal and power together         */
    uint16_t total_pellet_count;     /*!< What a freshly loaded maze holds  */
    cell_t pacman_start;
    cell_t pen_cells[PLAYFIELD_PEN_CELL_COUNT];
} playfield_t;

/* ==========================================================================
 * playfield - public API
 * ========================================================================= */

/*! \brief Load the maze, with every pellet restored.
 *
 * There is one maze and every level plays it (§10.2). It used to take a level number and
 * pick one of five layouts; the difficulty a level carries now lives in `difficulty`,
 * which is where the arcade puts it too.
 *
 * \param[out]      inout_playfield: playfield to load into, must not be `NULL`
 */
void playfield_load(playfield_t* inout_playfield);

/*! \brief Bring an out-of-range cell back inside the grid, wrapping each axis.
 *
 * This is the tunnel rule of FR-012 / §10.1: stepping off an edge re-enters at the
 * opposite edge on the same row or column. Wrapping is unconditional here; whether the
 * cell arrived at is passable is #playfield_is_walkable's business, so a maze without a
 * tunnel on that row simply has a wall waiting there.
 *
 * \param[in]       in_cell: cell to normalise, may be outside the grid
 * \return          The equivalent cell inside the grid
 */
cell_t playfield_wrap_cell(cell_t in_cell);

/*! \brief Whether an entity may stand on a cell.
 *
 * The cell is wrapped first, so a caller may pass a cell one step off the edge.
 *
 * \param[in]       in_playfield: loaded playfield, must not be `NULL`
 * \param[in]       in_cell: cell to test, may be outside the grid
 * \return          `true` when the cell is not a wall
 */
bool playfield_is_walkable(const playfield_t* in_playfield, cell_t in_cell);

/*! \brief Whether a cell is inside a tunnel.
 *
 * The tunnel is the stretch outside the maze body that the two edge mouths wrap onto. It
 * is marked in the map rather than derived, because "the cells of the row with the
 * tunnel" would also catch the corridor that merely leads to it — and the difference
 * matters: a ghost crawls through a tunnel at half its normal pace (§10.9), which is the
 * one place Pacman can reliably shake one off.
 *
 * \param[in]       in_playfield: loaded playfield, must not be `NULL`
 * \param[in]       in_cell: cell to test, may be outside the grid
 * \return          `true` when the cell is tunnel
 */
bool playfield_is_tunnel(const playfield_t* in_playfield, cell_t in_cell);

/*! \brief What is left to eat on a cell.
 *
 * \param[in]       in_playfield: loaded playfield, must not be `NULL`
 * \param[in]       in_cell: cell to inspect, may be outside the grid
 * \return          Member of \ref playfield_pellet_e
 */
playfield_pellet_e playfield_get_pellet(const playfield_t* in_playfield, cell_t in_cell);

/*! \brief Eat whatever is on a cell.
 *
 * \param[in,out]   inout_playfield: loaded playfield, must not be `NULL`
 * \param[in]       in_cell: cell to clear, may be outside the grid
 * \return          What was eaten, \ref PLAYFIELD_PELLET_NONE if the cell was already
 *                      empty
 */
playfield_pellet_e playfield_eat_pellet(playfield_t* inout_playfield, cell_t in_cell);

/*! \brief Pellets, normal and power together, still in the maze. */
uint16_t playfield_get_remaining_pellet_count(const playfield_t* in_playfield);

/*! \brief Pellets a freshly loaded maze holds — what "remaining" counts down from.
 *
 * Cruise Elroy (§10.9) triggers on how many are left, and the arcade's thresholds are
 * absolute counts against its own 244. Exposing the total lets that comparison be stated
 * against the maze actually loaded instead of a number baked into the rules.
 */
uint16_t playfield_get_total_pellet_count(const playfield_t* in_playfield);

/*! \brief Whether the maze has been cleared (FR-021). */
bool playfield_is_cleared(const playfield_t* in_playfield);

/*! \brief Where Pacman starts this level (§10.2). */
cell_t playfield_get_pacman_start(const playfield_t* in_playfield);

/*! \brief One of the ghost pen cells — also where an eaten ghost returns to (§10.5).
 *
 * \param[in]       in_playfield: loaded playfield
 * \param[in]       in_index: `0`..#PLAYFIELD_PEN_CELL_COUNT `- 1`
 */
cell_t playfield_get_pen_cell(const playfield_t* in_playfield, uint8_t in_index);

/*! \brief The maze corner a ghost scatters to (§10.4).
 *
 * Corners are returned in a fixed order so each ghost keeps its own: `0` top-left,
 * `1` top-right, `2` bottom-left, `3` bottom-right.
 *
 * \param[in]       in_index: `0`..`3`
 */
cell_t playfield_get_scatter_corner(uint8_t in_index);

/*! \brief Step one cell in a direction, with the tunnel wrap applied.
 *
 * Does not check walls — that is #playfield_is_walkable.
 *
 * \param[in]       in_cell: starting cell
 * \param[in]       in_direction: which way to step; \ref DIRECTION_NONE returns
 *                      `in_cell` unchanged
 * \return          The neighbouring cell, inside the grid
 */
cell_t playfield_step(cell_t in_cell, direction_e in_direction);

/*! \brief Manhattan distance between two cells (§10.4's ghost metric). */
uint16_t playfield_get_distance(cell_t in_first, cell_t in_second);

/*! \brief Whether two cells are the same. */
bool playfield_are_cells_equal(cell_t in_first, cell_t in_second);

/*! \brief The direction opposite the one given; \ref DIRECTION_NONE maps to itself. */
direction_e playfield_get_opposite_direction(direction_e in_direction);

#endif /* PLAYFIELD_H */
