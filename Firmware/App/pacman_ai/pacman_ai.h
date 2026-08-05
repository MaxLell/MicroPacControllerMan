/*
 * pacman_ai.h
 *
 * What the trained agent sees and what it may do
 * ([M6 §3/§4](../../../Docu/Design/M6-Pacman-AI.md), FR-035/FR-039).
 *
 * Everything here is expressed **relative to Pacman** — forward, left, right, back — rather
 * than in compass directions, and that is the whole point of the module. Every level's maze is
 * generated (FR-029), so a policy trained on absolute cell positions would have learned
 * nothing transferable; in Pacman's own frame, "wall ahead, corridor to the left" is one rule
 * instead of four.
 *
 * This code runs on the host **and** on the target, unchanged. It is what FR-039 pins the two
 * builds to: the same state must produce the same features and therefore the same action on
 * both. That is also why no `double` appears anywhere in it and why the arithmetic is written
 * out in a fixed order.
 *
 * Not this module's job: evaluating a network (that is `Services/neural_net`) or deciding when
 * to ask (that is the caller's frame). What *is* its job, besides the observation and the action
 * set, is owning the one trained network the firmware carries — #pacman_ai_decide puts the three
 * steps together, and `ai_weights.c` beside this file is the table it reads. The target evaluates
 * and never learns (FR-038): there is nothing here that writes a weight.
 */

#ifndef PACMAN_AI_H
#define PACMAN_AI_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"
#include "playfield.h"

/* ==========================================================================
 * pacman_ai - public types
 * ========================================================================= */

/*! \brief The four things the agent can ask for, relative to the way it is facing.
 *
 * `BACK` is in the set because Pacman may reverse — it is the *ghosts* that never do
 * ([DEC-037](../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)) — and an agent that
 * could not turn round would be stuck in any dead end. Generated mazes have none, but the
 * arcade map does and so might a future generator.
 *
 * The numbering is the tie-break order of \ref pacman_ai_choose_action: a lower value wins an
 * exact tie, so that two machines cannot resolve one differently (FR-039).
 */
typedef enum
{
    PACMAN_AI_ACTION_FORWARD = 0,
    PACMAN_AI_ACTION_LEFT,
    PACMAN_AI_ACTION_RIGHT,
    PACMAN_AI_ACTION_BACK,
    PACMAN_AI_ACTION_COUNT
} pacman_ai_action_e;

/*! \brief How many features one observation holds.
 *
 * Five per relative direction — is it open, and how far to the nearest pellet, power pellet,
 * dangerous ghost and edible ghost that way — plus three about the run as a whole.
 */
#define PACMAN_AI_FEATURES_PER_ACTION (5U)
#define PACMAN_AI_GLOBAL_FEATURES     (3U)
#define PACMAN_AI_FEATURE_COUNT                                                                                        \
    ((PACMAN_AI_FEATURES_PER_ACTION * (uint8_t)PACMAN_AI_ACTION_COUNT) + PACMAN_AI_GLOBAL_FEATURES)

/*! \brief What a distance feature holds when there is nothing of that kind to walk to.
 *
 * The same value a very distant one saturates at, which is deliberate: "no edible ghost
 * anywhere" and "an edible ghost on the far side of the maze" should mean the same thing to a
 * policy, because neither is worth turning for.
 */
#define PACMAN_AI_DISTANCE_NONE  (1.0F)

/*! \brief Cell distance at which a distance feature reaches #PACMAN_AI_DISTANCE_NONE.
 *
 * Chosen as the maze's own span rather than its cell count: a route longer than this is
 * across the whole board, and scaling by 868 would squeeze every interesting distance into
 * the bottom twentieth of the range.
 */
#define PACMAN_AI_DISTANCE_SCALE (PLAYFIELD_WIDTH + PLAYFIELD_HEIGHT)

/* ==========================================================================
 * pacman_ai - public API
 * ========================================================================= */

/*! \brief Describe the world as the agent sees it.
 *
 * Fills `out_features` with #PACMAN_AI_FEATURE_COUNT values, all in `[0, 1]`. The layout is
 * five features for each \ref pacman_ai_action_e in order, then the three global ones — see
 * [M6 §3](../../../Docu/Design/M6-Pacman-AI.md) for what each means.
 *
 * Distances are **maze** distances: a breadth-first walk over the open cells, counting the
 * tunnel wrap. Straight-line distance would lie exactly where it matters, because a wall
 * makes the nearest pellet the wrong way round.
 *
 * Only information the display shows the player is used (FR-035). Note in particular that the
 * frightened *timer* is not an input, because the player does not get one either — what the
 * player gets is the ghosts flashing, and so does the agent.
 *
 * **Not reentrant.** The breadth-first search works in file-scope scratch space rather than on
 * the stack, because the target reserves its buffers statically (NFR-008) and 868 cells of
 * queue and distance is more than belongs in a frame. Concurrent training runs in separate
 * processes, one environment stepped at a time within each.
 *
 * \param[in]       in_state: the game state, must not be `NULL`
 * \param[in]       in_playfield: the maze it is being played on, must not be `NULL`
 * \param[out]      out_features: #PACMAN_AI_FEATURE_COUNT floats, must not be `NULL`
 */
void pacman_ai_get_features(const msg_game_state_t* in_state, const playfield_t* in_playfield, float* out_features);

/*! \brief Which way a relative action points, for an agent facing `in_facing`.
 *
 * \ref DIRECTION_NONE as a facing is treated as north, so that an agent asked to act before
 * Pacman has ever moved gets a defined answer instead of an assertion. That case is real: a
 * level begins with Pacman stationary.
 *
 * \param[in]       in_action: what the agent chose
 * \param[in]       in_facing: the way Pacman is currently facing
 * \return          The absolute direction to hand to `game_set_direction`
 */
direction_e pacman_ai_action_to_direction(pacman_ai_action_e in_action, direction_e in_facing);

/*! \brief The action with the highest score, ties going to the lowest action index.
 *
 * The tie-break is part of the interface rather than an implementation detail: FR-039 compares
 * the host's chosen action against the target's, and two equal outputs must not be allowed to
 * decide differently on the two machines.
 *
 * \param[in]       in_scores: one score per action, must not be `NULL`
 * \return          Member of \ref pacman_ai_action_e
 */
pacman_ai_action_e pacman_ai_choose_action(const float* in_scores);

/*! \brief Whether the trained network can be evaluated at all.
 *
 * The table in `ai_weights.c` is generated, and a generator can be wrong; this asks
 * `neural_net_is_well_formed` about it and also that its shape matches this module's observation
 * and action set. Meant to be called once when a run takes the AI on, so that a bad table is
 * reported where a report is possible instead of showing up as an agent that plays badly.
 *
 * \return          `true` when #pacman_ai_decide may be called
 */
bool pacman_ai_is_available(void);

/*! \brief What the trained agent would do about this state — observation, network, action, in one.
 *
 * The whole decision, so that the target and the host take exactly the same path through it: this
 * is the function VT-INT-024 replays a recorded state set through (FR-039).
 *
 * \param[in]       in_state: the game state, must not be `NULL`
 * \param[in]       in_playfield: the maze it is being played on, must not be `NULL`
 * \return          The absolute direction to hand to `game_set_direction`
 */
direction_e pacman_ai_decide(const msg_game_state_t* in_state, const playfield_t* in_playfield);

#endif /* PACMAN_AI_H */
