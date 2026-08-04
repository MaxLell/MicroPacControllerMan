/*
 * msg_broker.h
 *
 * Publish/subscribe message bus — the only path between modules (FR-103, FR-107).
 * Specified in [03 §3.2](../../../Docu/PrePlanning/03-Architecture.md); adapted from
 * MovyDesk_Prototype's MessageBroker with the two changes that doc calls for:
 * subscribers register an **output queue** instead of a callback, and the broker is an
 * **object** rather than a singleton.
 *
 * Why those two matter:
 *
 * - **Queue, not callback.** A callback runs inside the publisher's stack, which makes
 *   a slow subscriber the publisher's problem and invites re-entrancy. A queue means a
 *   publisher never waits for a consumer, which is what the Active Object pattern
 *   requires (§3.5).
 * - **Object, not singleton.** Every call takes the instance as its first argument and
 *   all state lives inside it, so instances cannot interfere. One instance exists today
 *   (FR-110): the game's own bus, which carries its events to #score_t (§3.6). The
 *   firmware above the game reaches the next module by an ordinary call handing over a
 *   message type by value — see [03 §3.2](../../../Docu/PrePlanning/03-Architecture.md)
 *   for when a queue earns its keep and when it does not.
 *
 * The broker is content-agnostic: it routes on the topic ID and never looks at a
 * payload.
 *
 * **Threading.** There is none, and none is planned (DEC-027). Fan-out happens when the
 * owner asks for it: #msg_broker_process_all is called by the loop that owns the
 * instance — for the game, at the end of a tick — so an event published mid-tick reaches
 * its subscriber after that tick rather than inside it (FR-108).
 */

#ifndef MSG_BROKER_H
#define MSG_BROKER_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"
#include "msg_queue.h"

/* ==========================================================================
 * msg_broker - public types
 * ========================================================================= */

/*! \brief Subscribers that may register for one topic. */
#define MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC (4U)

typedef enum
{
    MSG_BROKER_STATUS_OK = 0,       /*!< Done                              */
    MSG_BROKER_STATUS_INPUT_FULL,   /*!< Publish rejected — apply backpressure */
    MSG_BROKER_STATUS_IDLE,         /*!< Nothing was waiting to be moved   */
    MSG_BROKER_STATUS_NO_SUBSCRIBER /*!< Delivered to nobody               */
} msg_broker_status_e;

/*! \brief A module's end of the bus: its own output queue, plus what it has missed. */
typedef struct
{
    msg_queue_t queue;
    uint32_t dropped_msg_count; /*!< Messages lost to a full queue     */
} msg_subscriber_t;

typedef struct
{
    msg_queue_t input_queue;
    msg_subscriber_t* topics[MSG_LAST][MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC];
    bool is_initialized;
    bool is_started;
    uint32_t published_msg_count; /*!< Accepted into the input queue     */
    uint32_t delivered_msg_count; /*!< Copies handed to subscribers      */
    uint32_t dropped_msg_count;   /*!< Copies lost to full output queues */
} msg_broker_t;

/* ==========================================================================
 * msg_subscriber - public API
 * ========================================================================= */

/*! \brief Give a subscriber its output queue.
 *
 * Call before subscribing to anything.
 *
 * \param[out]      inout_subscriber: subscriber to initialize, must not be `NULL`
 * \param[in]       inout_msg_buffer: array of at least `in_capacity` messages, borrowed
 *                      for the subscriber's lifetime
 * \param[in]       in_capacity: queue depth, at least `1`
 */
void msg_subscriber_init(msg_subscriber_t* inout_subscriber, msg_t* inout_msg_buffer, uint16_t in_capacity);

/*! \brief Take the next message from a subscriber's own queue.
 *
 * \param[in,out]   inout_subscriber: initialized subscriber
 * \param[out]      out_msg: receives the message, must not be `NULL`
 * \return          \ref MSG_BROKER_STATUS_OK when one was taken,
 *                      \ref MSG_BROKER_STATUS_IDLE when the queue was empty
 */
msg_broker_status_e msg_subscriber_receive(msg_subscriber_t* inout_subscriber, msg_t* out_msg);

/*! \brief How many messages this subscriber has lost to a full queue.
 *
 * Non-zero means this module is not draining fast enough. Worth logging rather than
 * ignoring — the broker deliberately drops for the slow subscriber instead of stalling
 * everyone else, so the loss is silent otherwise.
 */
uint32_t msg_subscriber_get_dropped_msg_count(const msg_subscriber_t* in_subscriber);

/* ==========================================================================
 * msg_broker - public API
 * ========================================================================= */

/*! \brief Initialize a broker instance and give it its input queue.
 *
 * \param[out]      inout_broker: instance to initialize, must not be `NULL`
 * \param[in]       inout_msg_buffer: array of at least `in_capacity` messages, borrowed
 *                      for the broker's lifetime
 * \param[in]       in_capacity: input queue depth, at least `1`
 */
void msg_broker_init(msg_broker_t* inout_broker, msg_t* inout_msg_buffer, uint16_t in_capacity);

/*! \brief Arm the broker so it will move messages (FR-108).
 *
 * Subscriptions are set up before this; #msg_broker_process refuses to run until it has
 * been called.
 */
void msg_broker_start(msg_broker_t* inout_broker);

/*! \brief Register a subscriber's queue for one topic.
 *
 * Subscribing the same subscriber to the same topic twice is a programming error.
 *
 * \param[in,out]   inout_broker: initialized instance
 * \param[in,out]   inout_subscriber: initialized subscriber
 * \param[in]       in_topic: topic between \ref MSG_NONE and \ref MSG_LAST
 */
void msg_broker_subscribe(msg_broker_t* inout_broker, msg_subscriber_t* inout_subscriber, msg_id_e in_topic);

/*! \brief Hand a message to the broker's input queue.
 *
 * Copies the message in and returns immediately — it is not delivered until
 * #msg_broker_process runs. Never blocks (NFR-105).
 *
 * \param[in,out]   inout_broker: started instance
 * \param[in]       in_msg: message to publish, must not be `NULL`
 * \return          \ref MSG_BROKER_STATUS_OK, or \ref MSG_BROKER_STATUS_INPUT_FULL when
 *                      the input queue is full — the publisher decides what to do, it
 *                      is not blocked
 */
msg_broker_status_e msg_broker_publish(msg_broker_t* inout_broker, const msg_t* in_msg);

/*! \brief Whether at least `in_headroom` more messages would fit in the input queue.
 *
 * The backpressure check of NFR-105: a publisher that can throttle asks first instead
 * of discovering a full queue after the fact.
 */
bool msg_broker_has_input_space(const msg_broker_t* in_broker, uint16_t in_headroom);

/*! \brief Move one message from the input queue to every subscriber of its topic.
 *
 * The body of the worker of FR-108. A subscriber whose queue is full has this message
 * dropped and counted; the others still get it, and the broker is not stalled.
 *
 * \param[in,out]   inout_broker: started instance
 * \return          \ref MSG_BROKER_STATUS_OK when a message was delivered to at least
 *                      one subscriber, \ref MSG_BROKER_STATUS_NO_SUBSCRIBER when it had
 *                      none, \ref MSG_BROKER_STATUS_IDLE when the input queue was empty
 */
msg_broker_status_e msg_broker_process(msg_broker_t* inout_broker);

/*! \brief Drain the input queue completely, for a super-loop that wants one call.
 *
 * \return          Number of messages moved
 */
uint32_t msg_broker_process_all(msg_broker_t* inout_broker);

/*! \brief Total copies the broker has lost to full subscriber queues. */
uint32_t msg_broker_get_dropped_msg_count(const msg_broker_t* in_broker);

#endif /* MSG_BROKER_H */
