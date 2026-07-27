#include "score.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "active_object.h"
#include "custom_assert.h"
#include "msg.h"
#include "msg_broker.h"

/* ==========================================================================
 * score - private
 * ========================================================================= */

#define SCORE_OBJECT_NAME "score"

/* 200, 400, 800, 1600 — the first ghost's value doubled for each further one caught in
 * the same frightened window (§10.6). */
#define SCORE_FIRST_GHOST_POINTS (200U)

static uint32_t prv_get_ghost_points(uint8_t in_chain_index)
{
    /* Beyond the fourth the value stops climbing: there are only four ghosts, so a fifth
     * would mean the chain was not reset when it should have been. */
    const uint8_t capped_index = (in_chain_index < SCORE_GHOST_CHAIN_LENGTH)
                                     ? in_chain_index
                                     : (uint8_t)(SCORE_GHOST_CHAIN_LENGTH - 1U);

    return (uint32_t)SCORE_FIRST_GHOST_POINTS << capped_index;
}

static void prv_handle_pellet_eaten(score_t* const inout_score, const msg_t* const in_msg)
{
    msg_pellet_eaten_t payload = {0};

    memcpy(&payload, in_msg->payload, sizeof(payload));

    inout_score->total
        += payload.is_power_pellet ? SCORE_POWER_PELLET_POINTS : SCORE_PELLET_POINTS;
}

static void prv_handle_msg(void* inout_context, const msg_t* in_msg)
{
    score_t* const score = inout_context;

    ASSERT(score != NULL);
    ASSERT(in_msg != NULL);

    switch (in_msg->id)
    {
        case MSG_GAME_PELLET_EATEN:
            prv_handle_pellet_eaten(score, in_msg);
            break;

        case MSG_GAME_GHOST_EATEN:
            score->total += prv_get_ghost_points(score->ghost_chain_index);

            if (score->ghost_chain_index < SCORE_GHOST_CHAIN_LENGTH)
            {
                ++score->ghost_chain_index;
            }
            break;

        case MSG_GAME_FRIGHTENED_STARTED:
            /* A fresh power pellet restarts the window, so the bonus starts over (§10.5). */
            score->ghost_chain_index = 0U;
            break;

        default:
            /* Subscribed only to the three above; anything else is a subscription bug. */
            ASSERT(false);
            break;
    }
}

/* ==========================================================================
 * score - public
 * ========================================================================= */

void score_init(score_t* inout_score, msg_broker_t* inout_broker)
{
    ASSERT(inout_score != NULL);
    ASSERT(inout_broker != NULL);

    inout_score->total = 0U;
    inout_score->ghost_chain_index = 0U;

    active_object_init(&inout_score->object, SCORE_OBJECT_NAME, inout_score->inbox,
                       SCORE_INBOX_CAPACITY, prv_handle_msg, inout_score);

    active_object_subscribe(&inout_score->object, inout_broker, MSG_GAME_PELLET_EATEN);
    active_object_subscribe(&inout_score->object, inout_broker, MSG_GAME_GHOST_EATEN);
    active_object_subscribe(&inout_score->object, inout_broker, MSG_GAME_FRIGHTENED_STARTED);
}

uint32_t score_process(score_t* inout_score)
{
    ASSERT(inout_score != NULL);

    return active_object_process_all(&inout_score->object);
}

uint32_t score_get_total(const score_t* in_score)
{
    ASSERT(in_score != NULL);

    return in_score->total;
}

uint32_t score_get_next_ghost_points(const score_t* in_score)
{
    ASSERT(in_score != NULL);

    return prv_get_ghost_points(in_score->ghost_chain_index);
}
