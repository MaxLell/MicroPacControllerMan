/*
 * score.h
 *
 * The running score and the scoring rules
 * ([10 §10.6](../../../Docu/PrePlanning/10-Pacman-Game-Design.md), A-006).
 *
 * An **Active Object on the game's internal broker** (§3.5, §3.6): it learns what happened
 * from events and nothing else. It never sees the maze, the positions or the ghosts, and
 * nothing can ask it to change — the only way in is a message. That makes it the clearest
 * example of what the internal bus is for, and it means the scoring rules can be verified
 * without a game running at all.
 *
 * The ghost bonus chain is the part with memory: four ghosts eaten inside one frightened
 * period are worth 200, 400, 800 and 1600 (§10.5/§10.6), and the chain resets when a new
 * power pellet restarts the window.
 */

#ifndef SCORE_H
#define SCORE_H

#include <stdint.h>

#include "active_object.h"
#include "msg.h"
#include "msg_broker.h"

/* ==========================================================================
 * score - public types
 * ========================================================================= */

/*! \brief Inbox depth. Generous next to the handful of events a tick can produce — at
 *         most one pellet plus four ghosts eaten. */
#define SCORE_INBOX_CAPACITY      (8U)

/*! \brief Points, per §10.6. */
#define SCORE_PELLET_POINTS       (10U)
#define SCORE_POWER_PELLET_POINTS (50U)

/*! \brief Ghosts eaten in one frightened period, before the bonus stops doubling. */
#define SCORE_GHOST_CHAIN_LENGTH  (4U)

typedef struct
{
    active_object_t object;
    msg_t inbox[SCORE_INBOX_CAPACITY];
    uint32_t total;
    uint8_t ghost_chain_index; /*!< How many eaten this frightened window */
} score_t;

/* ==========================================================================
 * score - public API
 * ========================================================================= */

/*! \brief Start a new run at zero and subscribe to the scoring events.
 *
 * \param[out]      inout_score: instance to reset, must not be `NULL`
 * \param[in,out]   inout_broker: the game's internal broker to subscribe on
 */
void score_init(score_t* inout_score, msg_broker_t* inout_broker);

/*! \brief Handle every scoring event waiting in the inbox.
 *
 * \param[in,out]   inout_score: initialized instance
 * \return          Number of events handled
 */
uint32_t score_process(score_t* inout_score);

/*! \brief The score so far. Cumulative across levels (§10.9). */
uint32_t score_get_total(const score_t* in_score);

/*! \brief What the next ghost eaten in this frightened period would be worth (§10.6).
 *
 * Exposed so the value can be checked and, later, shown to the player.
 */
uint32_t score_get_next_ghost_points(const score_t* in_score);

#endif /* SCORE_H */
