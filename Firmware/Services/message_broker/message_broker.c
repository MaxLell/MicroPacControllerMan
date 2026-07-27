#include "message_broker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "message.h"
#include "message_queue.h"

/* ==========================================================================
 * message_broker - private
 * ========================================================================= */

static void prv_assert_topic(message_id_e in_topic)
{
    ASSERT(in_topic > MESSAGE_ID_NONE);
    ASSERT(in_topic < MESSAGE_ID_LAST);
}

static void prv_assert_started(const message_broker_t* const in_broker)
{
    ASSERT(in_broker != NULL);
    ASSERT(in_broker->is_initialized);
    ASSERT(in_broker->is_started);
}

/* Deliver one message to every subscriber of its topic. Returns the number that took
 * it; a subscriber with a full queue is counted as dropped and skipped, so one slow
 * module cannot stall the broker or starve the others. */
static uint16_t prv_fan_out(message_broker_t* const inout_broker, const message_t* const in_message)
{
    uint16_t delivered = 0U;

    for (uint16_t slot = 0U; slot < MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        message_subscriber_t* const subscriber = inout_broker->topics[in_message->id][slot];

        if (subscriber == NULL)
        {
            continue;
        }

        if (message_queue_push(&subscriber->queue, in_message))
        {
            ++delivered;
            ++inout_broker->delivered_count;
        }
        else
        {
            ++subscriber->dropped_count;
            ++inout_broker->dropped_count;
        }
    }

    return delivered;
}

/* ==========================================================================
 * message_subscriber - public
 * ========================================================================= */

void message_subscriber_init(message_subscriber_t* inout_subscriber, message_t* inout_storage,
                             uint16_t in_capacity)
{
    ASSERT(inout_subscriber != NULL);

    message_queue_init(&inout_subscriber->queue, inout_storage, in_capacity);

    inout_subscriber->dropped_count = 0U;
}

message_broker_status_e message_subscriber_receive(message_subscriber_t* inout_subscriber,
                                                  message_t* out_message)
{
    ASSERT(inout_subscriber != NULL);
    ASSERT(out_message != NULL);

    if (!message_queue_pop(&inout_subscriber->queue, out_message))
    {
        return MESSAGE_BROKER_STATUS_IDLE;
    }

    return MESSAGE_BROKER_STATUS_OK;
}

uint32_t message_subscriber_get_dropped_count(const message_subscriber_t* in_subscriber)
{
    ASSERT(in_subscriber != NULL);

    return in_subscriber->dropped_count;
}

/* ==========================================================================
 * message_broker - public
 * ========================================================================= */

void message_broker_init(message_broker_t* inout_broker, message_t* inout_storage,
                         uint16_t in_capacity)
{
    ASSERT(inout_broker != NULL);

    /* Deliberately no "not already initialized" assertion, unlike the singleton this
     * was adapted from: on an instance that would read uninitialized memory, since a
     * caller may hand in a fresh stack or heap object. Init is the operation that
     * makes the state defined, so it cannot depend on it. */
    message_queue_init(&inout_broker->input_queue, inout_storage, in_capacity);

    for (uint16_t topic = 0U; topic < MESSAGE_ID_LAST; ++topic)
    {
        for (uint16_t slot = 0U; slot < MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
        {
            inout_broker->topics[topic][slot] = NULL;
        }
    }

    inout_broker->published_count = 0U;
    inout_broker->delivered_count = 0U;
    inout_broker->dropped_count = 0U;
    inout_broker->is_started = false;
    inout_broker->is_initialized = true;
}

void message_broker_start(message_broker_t* inout_broker)
{
    ASSERT(inout_broker != NULL);
    ASSERT(inout_broker->is_initialized);
    ASSERT(false == inout_broker->is_started);

    inout_broker->is_started = true;
}

void message_broker_subscribe(message_broker_t* inout_broker,
                              message_subscriber_t* inout_subscriber, message_id_e in_topic)
{
    ASSERT(inout_broker != NULL);
    ASSERT(inout_broker->is_initialized);
    ASSERT(inout_subscriber != NULL);
    prv_assert_topic(in_topic);

    for (uint16_t slot = 0U; slot < MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        /* Subscribing twice would deliver twice, which no handler expects. */
        ASSERT(inout_broker->topics[in_topic][slot] != inout_subscriber);
    }

    for (uint16_t slot = 0U; slot < MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        if (inout_broker->topics[in_topic][slot] == NULL)
        {
            inout_broker->topics[in_topic][slot] = inout_subscriber;

            return;
        }
    }

    /* Out of slots — raise MESSAGE_BROKER_MAX_SUBSCRIBERS_PER_TOPIC. */
    ASSERT(false);
}

message_broker_status_e message_broker_publish(message_broker_t* inout_broker,
                                              const message_t* in_message)
{
    prv_assert_started(inout_broker);
    ASSERT(in_message != NULL);
    prv_assert_topic(in_message->id);
    ASSERT(in_message->payload_size <= MESSAGE_PAYLOAD_MAX_SIZE);

    if (!message_queue_push(&inout_broker->input_queue, in_message))
    {
        /* Deliberately a status, not a block and not an assertion: a full queue is a
         * load condition the publisher is expected to handle (NFR-105). */
        return MESSAGE_BROKER_STATUS_INPUT_FULL;
    }

    ++inout_broker->published_count;

    return MESSAGE_BROKER_STATUS_OK;
}

bool message_broker_has_input_space(const message_broker_t* in_broker, uint16_t in_headroom)
{
    ASSERT(in_broker != NULL);
    ASSERT(in_broker->is_initialized);

    return message_queue_get_free_count(&in_broker->input_queue) >= in_headroom;
}

message_broker_status_e message_broker_process(message_broker_t* inout_broker)
{
    message_t message;

    prv_assert_started(inout_broker);

    if (!message_queue_pop(&inout_broker->input_queue, &message))
    {
        return MESSAGE_BROKER_STATUS_IDLE;
    }

    if (prv_fan_out(inout_broker, &message) == 0U)
    {
        /* Not an error: a topic nobody has subscribed to yet is normal during
         * bring-up, and the reference implementation had this assertion commented out
         * for the same reason. Reported so a caller can log it if it cares. */
        return MESSAGE_BROKER_STATUS_NO_SUBSCRIBER;
    }

    return MESSAGE_BROKER_STATUS_OK;
}

uint32_t message_broker_process_all(message_broker_t* inout_broker)
{
    uint32_t moved = 0U;

    prv_assert_started(inout_broker);

    while (message_broker_process(inout_broker) != MESSAGE_BROKER_STATUS_IDLE)
    {
        ++moved;
    }

    return moved;
}

uint32_t message_broker_get_dropped_count(const message_broker_t* in_broker)
{
    ASSERT(in_broker != NULL);

    return in_broker->dropped_count;
}
