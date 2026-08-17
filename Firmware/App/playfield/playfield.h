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
 * cell can be off-maze — §10.4's Pinky and Inky compute one, and every scatter target is
 * one on purpose — and the distance to it is still meaningful. Stepping is what wraps
 * (#playfield_wrap_cell).
 */

#ifndef PLAYFIELD_H
#define PLAYFIELD_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"

/* ==========================================================================
 * playfield - public types
 * ========================================================================= */

#define PLAYFIELD_WIDTH       (28)
#define PLAYFIELD_HEIGHT      (31)

/*! \brief Ghosts the maze holds a starting cell for, in the order
 *         `ghost_personality_e` numbers them (§10.2). */
#define PLAYFIELD_GHOST_COUNT (4)

typedef struct
{
    int16_t x; /*!< Column, `0` is the left edge      */
    int16_t y; /*!< Row, `0` is the top edge          */
} cell_t;

/* The map legend, one character per cell ([10 §10.2](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)).
 *
 * It is out here rather than private to `playfield.c` because it is a *contract* now: since
 * FR-029 the maze is generated, and `maze_gen` writes exactly these characters for
 * #playfield_load_from_map to read back. Two modules agreeing on a legend by eye is how a
 * wall turns into a pellet.
 *
 * `PLAYFIELD_MAP_GHOST_START_FIRST` is the first of #PLAYFIELD_GHOST_COUNT consecutive
 * digits, one per ghost, numbered as `ghost_personality_e` numbers them — Blinky, Pinky,
 * Inky, Clyde. Blinky's is the one *outside*, just above the gate, because that is where the
 * arcade puts him; the other three stand inside. Digits rather than initials because 'P' is
 * already Pacman's and 'C'/'I' would read as maze pieces. */
#define PLAYFIELD_MAP_WALL              '#'
#define PLAYFIELD_MAP_PELLET            '.'
#define PLAYFIELD_MAP_POWER_PELLET      'o'
#define PLAYFIELD_MAP_PACMAN_START      'P'
#define PLAYFIELD_MAP_HOUSE             'H'
#define PLAYFIELD_MAP_GATE              'D'
#define PLAYFIELD_MAP_TUNNEL            'T'
#define PLAYFIELD_MAP_OPEN              ' '
#define PLAYFIELD_MAP_GHOST_START_FIRST '0'

/*! \brief A maze as characters, one row per line, each `NUL`-terminated so a row is
 *         printable and comparable as a string.
 *
 * The type lives here rather than with the generator because it is `playfield`'s input
 * format, and it has three users who must agree on it: `maze_gen` writes one, this module
 * turns one into rules, and `game_view` derives the maze's *appearance* from one. */
typedef struct
{
    char rows[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH + 1];
} playfield_map_t;

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

    /*!< The ghost house, and the gate that is its only way in or out. Kept apart from the
     *   walls because the house is not a wall to anybody — it is a place only ghosts may
     *   be in, and only on their way out or after being eaten. */
    bool house[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    bool gates[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    playfield_pellet_e pellets[PLAYFIELD_HEIGHT][PLAYFIELD_WIDTH];
    uint16_t remaining_pellet_count; /*!< Normal and power together         */
    uint16_t total_pellet_count;     /*!< What a freshly loaded maze holds  */
    cell_t pacman_start;
    cell_t ghost_starts[PLAYFIELD_GHOST_COUNT];
} playfield_t;

/* ==========================================================================
 * playfield - public API
 * ========================================================================= */

/*! \brief Load the arcade's own maze, with every pellet restored.
 *
 * **This is the "normal maze" the menu offers** (FR-040), and it is also the fixture the generated
 * mazes are judged against — it is the one maze whose every property is known from outside this
 * codebase: 244 pellets, the corridors the Dossier's ghost behaviour was described against, the
 * tile map `game_view`'s appearance rules were checked with. For a while it was *only* the fixture,
 * between FR-029 giving every level a generated maze and DEC-045 giving the player the choice.
 *
 * \param[out]      inout_playfield: playfield to load into, must not be `NULL`
 */
void playfield_load(playfield_t* inout_playfield);

/*! \brief The arcade's own maze, as a map.
 *
 * For a caller that needs to *hand it somewhere* rather than load it — a test starting a run
 * on a maze whose corridors are known, which is the only way to assert about a corridor at
 * all once the real mazes are generated.
 *
 * \param[out]      out_map: filled in completely, must not be `NULL`
 */
void playfield_get_arcade_map(playfield_map_t* out_map);

/*! \brief Load a maze from a map, with every pellet restored.
 *
 * The map is read and not kept: everything the rules need is copied out, so a caller may
 * reuse or discard it immediately.
 *
 * A map that is missing a ghost start, or that holds no pellets, is a defect in whatever
 * produced it rather than a runtime condition, and asserts.
 *
 * \param[out]      inout_playfield: playfield to load into, must not be `NULL`
 * \param[in]       in_map: maze to load, must not be `NULL`
 */
void playfield_load_from_map(playfield_t* inout_playfield, const playfield_map_t* in_map);

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

/*! \brief Where a ghost starts a level (§10.2).
 *
 * Indexed as `ghost_personality_e` numbers them. Blinky's cell is **outside** the house,
 * just above the gate; the other three stand inside it, and it is also where an eaten one
 * is revived (§10.5).
 *
 * \param[in]       in_playfield: loaded playfield
 * \param[in]       in_index: `0`..#PLAYFIELD_GHOST_COUNT `- 1`
 */
cell_t playfield_get_ghost_start(const playfield_t* in_playfield, uint8_t in_index);

/*! \brief The cell just outside the gate — where a ghost leaving the house is headed, and
 *         where the arcade stands Blinky at the start of a level. */
cell_t playfield_get_house_exit(const playfield_t* in_playfield);

/*! \brief Whether a cell is inside the ghost house, gate included.
 *
 * The house is not a wall — it is a *place*, and who may be in it is a rule about actors
 * rather than about the maze. Pacman never may (§10.2); a ghost may only be on its way out
 * or after being eaten, and once outside it cannot get back in
 * ([10 §10.4](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)).
 */
bool playfield_is_house(const playfield_t* in_playfield, cell_t in_cell);

/*! \brief Whether a cell is the ghost house gate — the one cell pair that crossing the
 *         house boundary goes through, and therefore the only place the rule above has to
 *         be enforced. */
bool playfield_is_gate(const playfield_t* in_playfield, cell_t in_cell);

/*! \brief The fixed tile a ghost aims at while scattering (§10.4).
 *
 * Indexed as `ghost_personality_e` numbers them: Blinky top-right, Pinky top-left, Inky
 * bottom-right, Clyde bottom-left.
 *
 * **Deliberately outside the maze and therefore unreachable**, which is how the arcade gets
 * the behaviour: a ghost walks to the corner nearest its target and then, forbidden from
 * turning round, circles it until the mode changes. A reachable target would have it arrive
 * and stop being interesting.
 *
 * \param[in]       in_index: `0`..`3`
 */
cell_t playfield_get_scatter_target(uint8_t in_index);

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

/*! \brief The **square** of the straight-line distance between two cells (§10.4).
 *
 * Squared, and that is not an optimisation — it is what the arcade compares. Ghost
 * targeting only ever asks *which of these is nearer*, and squaring preserves that order
 * exactly while a square root would add a rounding step the original never had. Clyde's
 * eight-tile threshold is therefore compared against 64.
 *
 * It replaced a Manhattan distance, which is a different metric and picks a different
 * direction at a good many junctions: Manhattan makes a diagonal detour cost the same as
 * going straight, so a ghost would treat two visibly different options as equal and fall
 * through to the tie-break far more often than the original does.
 *
 * Returns 32 bits because a target may be well outside the maze — Pinky aims four cells
 * past Pacman and Inky doubles a vector, so the gap can be twice the board.
 */
uint32_t playfield_get_squared_distance(cell_t in_first, cell_t in_second);

/*! \brief Whether two cells are the same. */
bool playfield_are_cells_equal(cell_t in_first, cell_t in_second);

/*! \brief The direction opposite the one given; \ref DIRECTION_NONE maps to itself. */
direction_e playfield_get_opposite_direction(direction_e in_direction);

#endif /* PLAYFIELD_H */
