/*
 * msg.h
 *
 * The vocabulary every module shares: the topic IDs, the payload types, and the
 * fixed-size envelope they travel in. Transcribed from
 * [03 §3.3](../../../Docu/PrePlanning/03-Architecture.md) — that table is the
 * authority, this header is its code form.
 *
 * A message is a topic plus a small payload, **copied by value with no exceptions**, so
 * no module ever holds a pointer into another's memory and nothing needs the heap
 * (NFR-103). The render frame used to be the one exception, handing Render a pointer to a
 * double-buffered image; it is not any more. What is large is the rendered image, not the
 * game state — the state is 56 bytes and the image never travels, because Render draws
 * it (R-007 closed, DEC-016).
 *
 * Header-only: there is no msg.c, because a vocabulary has no behaviour.
 */

#ifndef MSG_H
#define MSG_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * msg - topics
 * ========================================================================= */

/*! \brief One value per topic, named as in [03 §3.3]. `MSG_NONE` and `MSG_LAST`
 *         bracket the range so a topic can be validated, and `MSG_LAST` sizes the
 *         broker's subscription table. */
typedef enum
{
    MSG_NONE = 0,
    MSG_INPUT_DIRECTION,     /*!< Input  -> Game                    */
    MSG_INPUT_BUTTON,        /*!< Input  -> System                  */
    MSG_SYSTEM_SHOW_LOADING, /*!< System -> Render                  */
    MSG_SYSTEM_SHOW_MENU,    /*!< System -> Render                  */
    MSG_SYSTEM_START_GAME,   /*!< System -> Game                    */
    MSG_GAME_STATE,          /*!< Game   -> Game-View               */
    MSG_DISPLAY_LIST,        /*!< Game-View -> Render                */
    MSG_GAME_SCORE_UPDATED,  /*!< Game   -> NVM                     */
    MSG_GAME_OVER,           /*!< Game   -> System, NVM             */
    MSG_HIGHSCORE_LOADED,    /*!< NVM    -> System                  */

    /* Game-internal topics — the second broker instance of FR-110 ([03 §3.6]). They never
     * appear on the system bus: only the Game module bridges the two, forwarding results
     * outward. These carry *events*, not queries and not per-tick commands — resolving a
     * tick is inherently sequential (move, eat, then collide in the same tick, §10.7), so
     * the orchestrator does that synchronously and publishes what happened. */
    MSG_GAME_PELLET_EATEN,       /*!< Game   -> Score                    */
    MSG_GAME_GHOST_EATEN,        /*!< Game   -> Score                    */
    MSG_GAME_FRIGHTENED_STARTED, /*!< Game   -> Score (resets the chain)  */

    MSG_LAST
} msg_id_e;

/* ==========================================================================
 * msg - payloads
 * ========================================================================= */

/*! \brief The four ghosts; Pacman is carried separately. */
#define MSG_GHOST_COUNT       (4U)

/*! \brief Everything that moves — the four ghosts plus Pacman. What a frame of drawing
 *         must always be able to carry in one message. */
#define MSG_ACTOR_COUNT       (MSG_GHOST_COUNT + 1U)

/*! \brief The playfield, in cells ([10 §10.2](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)),
 *         and the bytes needed to hold one bit per cell. */
#define MSG_PLAYFIELD_COLUMNS (11U)
#define MSG_PLAYFIELD_ROWS    (9U)
#define MSG_CELL_BITMAP_BYTES (((MSG_PLAYFIELD_COLUMNS * MSG_PLAYFIELD_ROWS) + 7U) / 8U)

/*! \brief Directions Pacman can be sent in. `NONE` means "no direction yet". */
typedef enum
{
    DIRECTION_NONE = 0,
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST
} direction_e;

/*! \brief Payload of \ref MSG_INPUT_DIRECTION (FR-004). */
typedef struct
{
    direction_e direction;
} msg_input_direction_t;

/*! \brief Payload of \ref MSG_INPUT_BUTTON (FR-003). */
typedef struct
{
    bool is_pressed;
} msg_input_button_t;

/*! \brief Payload of \ref MSG_SYSTEM_SHOW_MENU and \ref MSG_HIGHSCORE_LOADED
 *         (FR-002, FR-009). */
typedef struct
{
    uint32_t high_score;
} msg_high_score_t;

/*! \brief Payload of \ref MSG_GAME_PELLET_EATEN (§10.6). */
typedef struct
{
    bool is_power_pellet;
} msg_pellet_eaten_t;

/*! \brief Payload of \ref MSG_GAME_SCORE_UPDATED. */
typedef struct
{
    uint32_t score;
} msg_game_score_t;

/*! \brief Payload of \ref MSG_GAME_OVER (FR-007, FR-021). */
typedef struct
{
    uint32_t final_score;
    bool has_won;
} msg_game_over_t;

/*! \brief How far an entity has travelled from its current cell towards the next,
 *         in 1/256ths. `0` means "on the cell".
 *
 * The rules never read this. It exists so the view can draw motion in pixels while the
 * logic stays on whole cells: at 21 px per cell and 150 ms to cross one, drawing
 * cell-by-cell is a 21-pixel jump 6.7 times a second, which is visibly choppy however
 * fast the panel refreshes. See [10 §10.1](../../../Docu/PrePlanning/10-Pacman-Game-Design.md).
 */
typedef uint8_t cell_progress_t;

/*! \brief One moving entity, as the view needs to see it.
 *
 * Four bytes, deliberately: `direction` holds a \ref direction_e narrowed to a byte
 * rather than the enum itself. Five of these plus the pellet bits are the whole payload,
 * and an `int`-sized enum here would triple the actor block for nothing.
 */
typedef struct
{
    uint8_t column; /*!< Current cell, the one the rules act on     */
    uint8_t row;
    uint8_t direction;        /*!< A \ref direction_e                          */
    cell_progress_t progress; /*!< How far towards the next cell (view only)  */
} msg_actor_t;

/*! \brief Payload of \ref MSG_GAME_STATE (FR-005): one coherent state per simulation
 *         step, copied like every other payload.
 *
 * This is the whole dynamic state of the game. The maze itself is not here — it is
 * static per level, so the view owns the five tables and needs only the level number.
 */
typedef struct
{
    msg_actor_t pacman;
    msg_actor_t ghosts[MSG_GHOST_COUNT];
    /* Two bitmaps rather than one, because a pellet has three states, not two: gone,
     * normal, or power. `is_power` is only meaningful where `has_pellet` is set. A power
     * pellet is drawn larger and it is what starts frightened mode, so the view cannot
     * treat the two alike. */
    uint8_t has_pellet[MSG_CELL_BITMAP_BYTES];
    uint8_t is_power[MSG_CELL_BITMAP_BYTES];
    uint32_t score;
    uint8_t lives;
    uint8_t level;

    /* One bit per ghost, not one flag for the game: a ghost that has been eaten sits in
     * the pen un-frightened while the others are still blue, so a single flag would
     * either colour it wrongly or un-colour the rest. */
    uint8_t frightened_ghosts;
} msg_game_state_t;

/*! \brief What one entry of a display list asks for.
 *
 * Two kinds, and the difference is entirely about *erasing*. An actor moves, so Render
 * has to put back whatever the sprite covered before drawing it somewhere else; a
 * background item is a change to the field itself, which stays until something changes
 * it again. Render can tell them apart only if the message says so.
 */
typedef enum
{
    DISPLAY_ITEM_NONE = 0,
    DISPLAY_ITEM_BACKGROUND, /*!< A field tile: a wall, a pellet, or emptiness   */
    DISPLAY_ITEM_ACTOR       /*!< Moves; Render saves what it covers and restores it */
} display_item_kind_e;

/*! \brief One thing to draw: a sprite, a palette, and where it goes in pixels.
 *
 * Uniform on purpose. A wall, an eaten pellet's empty tile and Pacman are all "this
 * drawing, that palette, here", which is what keeps Render free of the maze, the tile
 * size and the screen layout — all of that stays in Game-View, where it can be tested
 * without a display.
 */
typedef struct
{
    uint8_t kind;    /*!< A \ref display_item_kind_e                        */
    uint8_t sprite;  /*!< A `sprite_set_id_e`; this header does not name them */
    uint8_t palette; /*!< A `sprite_set_palette_e`                            */
    uint8_t reserved;
    int16_t x; /*!< Left edge, in panel pixels                          */
    int16_t y; /*!< Top edge, in panel pixels                           */
} msg_display_item_t;

/*! \brief How many items travel in one message.
 *
 * Eight: the five actors of a frame, plus room for the field cells that changed under
 * them. A frame has to fit in **one** message — split across two, a reader could draw
 * half of it and show a pellet that Pacman has already eaten. Drawing a whole field on a
 * level change takes 99 tiles and therefore many messages, but that is a transition, not
 * a frame, and nobody is watching it. */
#define MSG_DISPLAY_ITEM_MAX (8U)

/*! \brief Payload of \ref MSG_DISPLAY_LIST (FR-005). */
typedef struct
{
    uint8_t count;
    msg_display_item_t items[MSG_DISPLAY_ITEM_MAX];
} msg_display_list_t;

/* ==========================================================================
 * msg - the cell bitmaps
 * ========================================================================= */

/*! \brief Index of a cell in a \ref MSG_CELL_BITMAP_BYTES bitmap.
 *
 * These two are the exception to "a vocabulary has no behaviour", and they earn it: the
 * writer and the reader of a bitmap are different modules, and bit arithmetic duplicated
 * on both sides is a disagreement waiting to happen.
 */
static inline bool msg_cell_bitmap_get(const uint8_t* in_bitmap, uint8_t in_column, uint8_t in_row)
{
    const uint16_t index = (uint16_t)((in_row * MSG_PLAYFIELD_COLUMNS) + in_column);

    return (in_bitmap[index / 8U] & (uint8_t)(1U << (index % 8U))) != 0U;
}

static inline void msg_cell_bitmap_set(uint8_t* inout_bitmap, uint8_t in_column, uint8_t in_row, bool in_is_set)
{
    const uint16_t index = (uint16_t)((in_row * MSG_PLAYFIELD_COLUMNS) + in_column);
    const uint8_t mask = (uint8_t)(1U << (index % 8U));

    if (in_is_set)
    {
        inout_bitmap[index / 8U] |= mask;
    }
    else
    {
        inout_bitmap[index / 8U] &= (uint8_t)~mask;
    }
}

/* ==========================================================================
 * msg - envelope
 * ========================================================================= */

/*! \brief Size of the payload area. Must hold the largest payload above; the static
 *         assertions below fail the build if a new payload outgrows it.
 *
 * 16 bytes sufficed while the largest payload was a pointer. The two that set the size
 * now are the whole game state and a whole frame of drawing — which is the point: both
 * are small enough to copy, so nothing has to be shared. */
#define MSG_PAYLOAD_MAX_SIZE (72U)

typedef struct
{
    msg_id_e id;                           /*!< Topic, a member of \ref msg_id_e  */
    uint16_t payload_size;                 /*!< Valid bytes in `payload`          */
    uint8_t payload[MSG_PAYLOAD_MAX_SIZE]; /*!< Opaque to the broker              */
} msg_t;

_Static_assert(sizeof(msg_input_direction_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_input_button_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_high_score_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_game_score_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_pellet_eaten_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_game_over_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_game_state_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_display_list_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");

#endif /* MSG_H */
