/*
 * message_broker.h
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
 *   requires (§3.5, FR-109).
 * - **Object, not singleton.** Every call takes the instance as its first argument and
 *   all state lives inside it, so instances cannot interfere. The project uses two
 *   (FR-110): a system broker between the firmware modules, and one used only inside
 *   the game (§3.6).
 *
 * The broker is content-agnostic: it routes on the topic ID and never looks at a
 * payload.
 *
 * **Threading.** FR-108 calls for a dedicated worker task per broker. FreeRTOS arrives
 * in M4, so for now #message_broker_process is that worker's body and the super-loop
 * calls it. The API does not change when the task appears.
 */

#ifndef MESSAGE_BROKER_H
#define MESSAGE_BROKER_H

#include <stdbool.h>
#include <stdint.h>

#include "message.h"
#include "message_queue.h"

/* ==========================================================================
 * message_broker - public types
 * ========================================================================= */

/*! \brief Subscribers that may register for one topic. */
#define MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC (4U)

typedef enum
{
    MESSAGE_BROKER_STATUS_OK = 0,               /*!< Done                              */
    MESSAGE_BROKER_STATUS_INPUT_FULL,           /*!< Publish rejected — apply backpressure */
    MESSAGE_BROKER_STATUS_IDLE,                 /*!< Nothing was waiting to be moved   */
    MESSAGE_BROKER_STATUS_NO_SUBSCRIBER         /*!< Delivered to nobody               */
} message_broker_status_e;

/*! \brief A module's end of the bus: its own output queue, plus what it has missed. */
typedef struct
{
    message_queue_t queue;
    uint32_t dropped_count;                     /*!< Messages lost to a full queue     */
} message_subscriber_t;

typedef struct
{
    message_queue_t input_queue;
    message_subscriber_t* topics[MESSAGE_ID_LAST][MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC];
    bool is_initialized;
    bool is_started;
    uint32_t published_count;                   /*!< Accepted into the input queue     */
    uint32_t delivered_count;                   /*!< Copies handed to subscribers      */
    uint32_t dropped_count;                     /*!< Copies lost to full output queues */
} message_broker_t;

/* ==========================================================================
 * message_subscriber - public API
 * ========================================================================= */

/*! \brief Give a subscriber its output queue.
 *
 * Call before subscribing to anything.
 *
 * \param[out]      inout_subscriber: subscriber to initialize, must not be `NULL`
 * \param[in]       inout_storage: array of at least `in_capacity` messages, borrowed
 *                      for the subscriber's lifetime
 * \param[in]       in_capacity: queue depth, at least `1`
 */
void message_subscriber_init(message_subscriber_t* inout_subscriber, message_t* inout_storage,
                             uint16_t in_capacity);

/*! \brief Take the next message from a subscriber's own queue.
 *
 * \param[in,out]   inout_subscriber: initialized subscriber
 * \param[out]      out_message: receives the message, must not be `NULL`
 * \return          \ref MESSAGE_BROKER_STATUS_OK when one was taken,
 *                      \ref MESSAGE_BROKER_STATUS_IDLE when the queue was empty
 */
message_broker_status_e message_subscriber_receive(message_subscriber_t* inout_subscriber,
                                                  message_t* out_message);

/*! \brief How many messages this subscriber has lost to a full queue.
 *
 * Non-zero means this module is not draining fast enough. Worth logging rather than
 * ignoring — the broker deliberately drops for the slow subscriber instead of stalling
 * everyone else, so the loss is silent otherwise.
 */
uint32_t message_subscriber_get_dropped_count(const message_subscriber_t* in_subscriber);

/* ==========================================================================
 * message_broker - public API
 * ========================================================================= */

/*! \brief Initialize a broker instance and give it its input queue.
 *
 * \param[out]      inout_broker: instance to initialize, must not be `NULL`
 * \param[in]       inout_storage: array of at least `in_capacity` messages, borrowed
 *                      for the broker's lifetime
 * \param[in]       in_capacity: input queue depth, at least `1`
 */
void message_broker_init(message_broker_t* inout_broker, message_t* inout_storage,
                         uint16_t in_capacity);

/*! \brief Arm the broker so it will move messages (FR-108).
 *
 * Subscriptions are set up before this; #message_broker_process refuses to run until
 * it has been called. Once FreeRTOS is in, this is where the worker task starts.
 */
void message_broker_start(message_broker_t* inout_broker);

/*! \brief Register a subscriber's queue for one topic.
 *
 * Subscribing the same subscriber to the same topic twice is a programming error.
 *
 * \param[in,out]   inout_broker: initialized instance
 * \param[in,out]   inout_subscriber: initialized subscriber
 * \param[in]       in_topic: topic between \ref MESSAGE_ID_NONE and \ref MESSAGE_ID_LAST
 */
void message_broker_subscribe(message_broker_t* inout_broker,
                              message_subscriber_t* inout_subscriber, message_id_e in_topic);

/*! \brief Hand a message to the broker's input queue.
 *
 * Copies the message in and returns immediately — it is not delivered until
 * #message_broker_process runs. Never blocks (NFR-105).
 *
 * \param[in,out]   inout_broker: started instance
 * \param[in]       in_message: message to publish, must not be `NULL`
 * \return          \ref MESSAGE_BROKER_STATUS_OK, or
 *                      \ref MESSAGE_BROKER_STATUS_INPUT_FULL when the input queue is
 *                      full — the publisher decides what to do, it is not blocked
 */
message_broker_status_e message_broker_publish(message_broker_t* inout_broker,
                                              const message_t* in_message);

/*! \brief Whether at least `in_headroom` more messages would fit in the input queue.
 *
 * The backpressure check of NFR-105: a publisher that can throttle asks first instead
 * of discovering a full queue after the fact.
 */
bool message_broker_has_input_space(const message_broker_t* in_broker, uint16_t in_headroom);

/*! \brief Move one message from the input queue to every subscriber of its topic.
 *
 * The body of the worker of FR-108. A subscriber whose queue is full has this message
 * dropped and counted; the others still get it, and the broker is not stalled.
 *
 * \param[in,out]   inout_broker: started instance
 * \return          \ref MESSAGE_BROKER_STATUS_OK when a message was delivered to at
 *                      least one subscriber, \ref MESSAGE_BROKER_STATUS_NO_SUBSCRIBER
 *                      when it had none, \ref MESSAGE_BROKER_STATUS_IDLE when the input
 *                      queue was empty
 */
message_broker_status_e message_broker_process(message_broker_t* inout_broker);

/*! \brief Drain the input queue completely, for a super-loop that wants one call.
 *
 * \return          Number of messages moved
 */
uint32_t message_broker_process_all(message_broker_t* inout_broker);

/*! \brief Total copies the broker has lost to full subscriber queues. */
uint32_t message_broker_get_dropped_count(const message_broker_t* in_broker);

#endif /* MESSAGE_BROKER_H */
