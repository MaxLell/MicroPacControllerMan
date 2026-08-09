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

/*! \brief Side of one maze cell on the panel, in pixels — the arcade's 8.
 *
 * 28 cells across is 224 px of a 240 px panel, and 31 down is 248 of 320. That is the
 * arcade's own playfield at its native scale, which is what FR-022 asks for and what the
 * 128 px panel this project started on could not hold. */
#define GAME_VIEW_TILE_SIZE        (8)

/*! \brief An actor sprite spans two cells, as the arcade's do, and is therefore drawn
 *         half a cell up and to the left of the cell it occupies. */
#define GAME_VIEW_ACTOR_SIZE       (16)

/*! \brief Where the maze's top-left corner sits on the panel. Horizontally centred; the
 *         248 px of maze start three cells down, so the rows above and below are free for
 *         the score and lives — the same places the arcade puts them. */
#define GAME_VIEW_ORIGIN_X         ((FRAMEBUFFER_WIDTH - (PLAYFIELD_WIDTH * GAME_VIEW_TILE_SIZE)) / 2)
#define GAME_VIEW_ORIGIN_Y         (24)

/*! \brief One cell of wall drawn *outside* the maze, all the way round it.
 *
 * The maze's outer wall is one cell thick, and one cell cannot carry the same line as the
 * rest of the maze: every wall inside is two cells thick and draws a 6-pixel stroke set 5
 * pixels in from its edge, which needs 16 pixels to sit in. Squeezed into 8 the stroke has to
 * move, and then the corridor beside it is narrower than every other corridor, its pellets sit
 * off the middle, and the joins where a wall meets it do not line up
 * ([DEC-033](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)).
 *
 * So the outer wall is given its second cell in the panel's own margin — 8 px either side of a
 * 224 px maze in a 240 px panel, and the row above. It is then an ordinary two-cell wall and
 * needs no rules, no tiles and no exceptions of its own. */
#define GAME_VIEW_MAZE_BORDER      (1)

/*! \brief The drawn field, which is the maze plus that border. */
#define GAME_VIEW_FIELD_WIDTH      (PLAYFIELD_WIDTH + (2 * GAME_VIEW_MAZE_BORDER))
#define GAME_VIEW_FIELD_HEIGHT     (PLAYFIELD_HEIGHT + (2 * GAME_VIEW_MAZE_BORDER))

/*! \brief The HUD, in the rows the maze leaves free above and below it (FR-022).
 *
 * Three rows above and six below, and the arcade's own arrangement in them: a label on the
 * first row and its value on the second, score at the left, and the lives as little
 * Pacmans along the bottom. Where the arcade puts a fruit for the level, this puts the
 * number — there is no fruit in this game
 * ([01 §1.2](../../../Docu/PrePlanning/01-System-Overview-and-Context.md)).
 *
 * Seven digits of score, because a full run can pass a million; two of level, because the
 * last one is 21.
 */
#define GAME_VIEW_HUD_LABEL_ROW_Y  (0)
#define GAME_VIEW_HUD_VALUE_ROW_Y  (GAME_VIEW_TILE_SIZE)
/* One cell lower than the maze ends, because the border is drawn there now. */
#define GAME_VIEW_HUD_LIVES_Y      (GAME_VIEW_ORIGIN_Y + ((PLAYFIELD_HEIGHT + GAME_VIEW_MAZE_BORDER) * GAME_VIEW_TILE_SIZE))

#define GAME_VIEW_HUD_SCORE_DIGITS (7U)
#define GAME_VIEW_HUD_LEVEL_DIGITS (2U)

/*! \brief How many lives the HUD has room to show. A run that starts with more than this
 *         is drawn as this many — the number is the model's, the picture is ours. */
#define GAME_VIEW_HUD_LIFE_SLOTS   (3U)

/*! \brief The name of whichever machine is playing (FR-032), in the gap the label row leaves
 *         between `1UP` and `LEVEL`.
 *
 * **Six, because the name is the point.** It used to be two and to read `AI`, which answers "is a
 * machine playing" — and once there are two machines to choose between, that leaves the player
 * remembering which one they picked on the menu. `NEAT` and `SEARCH` say it instead. Six is what
 * the longer of the two needs, and the label row has sixteen free columns between `1UP` and
 * `LEVEL`, so it costs nothing that was being used.
 *
 * Always six slots, whether or not a machine is playing: while none is, they draw the font's space.
 * A slot that stopped being described would leave the last thing it drew on the panel for the rest
 * of the run — the same reason a spent life is drawn as a blank rather than skipped.
 */
#define GAME_VIEW_HUD_AI_SLOTS     (6U)

/*! \brief The `LOOP` the HUD shows while the endless mode is on (FR-043), on the lives row and
 *         right-aligned under `LEVEL`.
 *
 * A field of its own rather than more letters next to `AI`, because the two say different things: one
 * is who is playing, the other is what happens when this run ends. Four slots always, blank while
 * the loop is off, for the same reason as the AI's two.
 */
#define GAME_VIEW_HUD_LOOP_SLOTS   (4U)

/*! \brief Everything the HUD draws, as a fixed list: `1UP`, the score, `LEVEL`, the level,
 *         the life slots, the AI indication and the loop indication. Fixed on purpose — a list that
 *         never changes length can be compared against what was last drawn item by item. */
#define GAME_VIEW_HUD_ITEM_COUNT                                                                                       \
    (3U + GAME_VIEW_HUD_SCORE_DIGITS + 5U + GAME_VIEW_HUD_LEVEL_DIGITS + GAME_VIEW_HUD_LIFE_SLOTS                      \
     + GAME_VIEW_HUD_AI_SLOTS + GAME_VIEW_HUD_LOOP_SLOTS)

/*! \brief The `drawn_hud` entry for a slot that has never been drawn. Above every
 *         `sprite_set_id_e`, so the first comparison always reports a change. */
#define GAME_VIEW_HUD_NOT_DRAWN (0xFFU)

typedef struct
{
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
    /* 28 x 31 is 868 cells, so this counts past what a byte holds — it did not while the
     * maze was 11 x 9, and the compiler said so the moment the grid grew. */
    uint16_t pending_field_cell;
    bool is_full_field_pending;

    /*!< Which drawing each HUD slot last showed, so only the digits that actually moved go
     *   out. The score changes on almost every pellet, and re-sending seven digits each
     *   time would cost more of the frame than the five actors do. */
    uint8_t drawn_hud[GAME_VIEW_HUD_ITEM_COUNT];

    /*!< The maze being drawn. Held rather than turned into pictures up front: a wall cell's
     *   pixels are arithmetic on where the walls are, and the only time they are all wanted at
     *   once is the handover after a new maze. */
    playfield_map_t maze;
    bool has_maze;

    /*!< Whether the HUD says the AI has taken over (FR-032). Told to the view rather than read
     *   out of the state message: taking over is not something the *game* knows about, and
     *   `msg_game_state_t` describes the game. The diffing needs no help with it — a slot is
     *   compared against the sprite that was last drawn there, not against an older state, so
     *   flipping this redraws the two slots on the next frame and nothing else. */
    /*! \brief Which of the three is steering, so the HUD can say *which* machine it is.
     *
     * A `uint8_t` rather than the session's own enumeration, because the view is below the session
     * and must not include it — the same reason nothing here knows what a `game_t` is. The values
     * are `game_session_player_e`'s and \ref game_view_set_player is where they arrive. */
    uint8_t player;

    /*!< Whether a finished run will be followed by another (FR-043). The view only draws it; the
     *   shell owns whether it is true. */
    bool is_infinite;
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

/*! \brief Take the maze that is about to be played, and work out how to draw it.
 *
 * Called once per maze, before the first state of that maze. The appearance is **derived**
 * from where the walls are: which of the arcade's thirty-odd line, corner and junction
 * pieces goes in a cell follows from which of its neighbours are open, so a generated maze
 * (FR-029) draws itself with no second map to keep in step.
 *
 * That is a change of principle. The maze used to be written down twice — the rules in
 * `playfield`, the appearance here — with a unit test holding the two together, because a
 * hand-drawn tile map cannot be derived from a wall bitmap. Once the maze is generated there
 * is nothing to hand-draw, so the derivation replaces the copy and the test now checks the
 * derivation against the arcade's own map instead.
 *
 * Forces the whole field to be described again: a new maze is every cell changed.
 *
 * \param[in,out]   inout_view: the view, must not be `NULL`
 * \param[in]       in_map: the maze, in `playfield`'s legend; must not be `NULL`
 */
void game_view_set_maze(game_view_t* inout_view, const playfield_map_t* in_map);

/*! \brief Take a new game state.
 *
 * \param[in,out]   inout_view: the view, must not be `NULL`
 * \param[in]       in_state: the state, copied in; must not be `NULL`
 */
void game_view_set_state(game_view_t* inout_view, const msg_game_state_t* in_state);

/*! \brief Say whether the HUD should show that the AI has taken over (FR-032).
 *
 * \param[in,out]   inout_view: the view, must not be `NULL`
 * \param[in]       in_is_active: `true` while the agent is playing
 */
/*! \brief Say who is steering, so the HUD can label it (FR-032).
 *
 * `AI` while the trained network plays and `LA` while the look-ahead search does — two glyphs
 * either way, so the slot the HUD already reserved is enough and no layout moves. Pac-Man is drawn
 * in the agent's colour for both: the colour answers "is this still me", which is one question,
 * and the label answers which machine, which is the other.
 *
 * \param[in,out]   inout_view: instance, must not be `NULL`
 * \param[in]       in_player: a `game_session_player_e`
 */
void game_view_set_player(game_view_t* inout_view, uint8_t in_player);

/*! \brief Tell the view whether the endless mode is on, so the HUD can say so (FR-043).
 *
 * \param[in,out]   inout_view: initialised view, must not be `NULL`
 * \param[in]       in_is_infinite: `true` while a finished run will be followed by another
 */
void game_view_set_infinite(game_view_t* inout_view, bool in_is_infinite);

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

/*! \brief Whether this cell is drawn as a piece of maze wall.
 *
 * The ghost house gate is **not** a wall by this measure: it is drawn, but a ghost may
 * cross it, which is the one place the picture and the rules disagree on purpose.
 *
 * Neither is the inside of a wall block: the arcade draws a wall as a thin outline with
 * nothing inside it, so most of a thick wall is drawn as emptiness while remaining solid to
 * the rules. "Drawn as wall" means wall *art*, not "impassable".
 *
 * \param[in]       in_view: the view, with a maze set, must not be `NULL`
 * \param[in]       in_column: cell column, below \ref PLAYFIELD_WIDTH
 * \param[in]       in_row: cell row, below \ref PLAYFIELD_HEIGHT
 */
bool game_view_is_wall_drawn_at(const game_view_t* in_view, uint8_t in_column, uint8_t in_row);

#endif /* GAME_VIEW_H */
