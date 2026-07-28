#include "msg_broker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "msg.h"
#include "msg_queue.h"

/* ==========================================================================
 * msg_broker - private
 * ========================================================================= */

static void prv_assert_topic(msg_id_e in_topic)
{
    ASSERT(in_topic > MSG_NONE);
    ASSERT(in_topic < MSG_LAST);
}

static void prv_assert_started(const msg_broker_t* const in_broker)
{
    ASSERT(in_broker != NULL);
    ASSERT(in_broker->is_initialized);
    ASSERT(in_broker->is_started);
}

/* Copy one message into the output queue of every subscriber registered for its topic.
 *
 * Returns how many subscribers took it. A subscriber whose queue is full has this
 * message counted as dropped and is skipped, so one module that is not draining cannot
 * stall the broker or starve the others. */
static uint16_t prv_copy_msg_to_subscribers(msg_broker_t* const inout_broker, const msg_t* const in_msg)
{
    uint16_t delivered_count = 0U;

    for (uint16_t slot = 0U; slot < MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        msg_subscriber_t* const subscriber = inout_broker->topics[in_msg->id][slot];

        if (subscriber == NULL)
        {
            continue;
        }

        if (msg_queue_push(&subscriber->queue, in_msg))
        {
            ++delivered_count;
            ++inout_broker->delivered_msg_count;
        }
        else
        {
            ++subscriber->dropped_msg_count;
            ++inout_broker->dropped_msg_count;
        }
    }

    return delivered_count;
}

/* ==========================================================================
 * msg_subscriber - public
 * ========================================================================= */

void msg_subscriber_init(msg_subscriber_t* inout_subscriber, msg_t* inout_msg_buffer, uint16_t in_capacity)
{
    ASSERT(inout_subscriber != NULL);

    msg_queue_init(&inout_subscriber->queue, inout_msg_buffer, in_capacity);

    inout_subscriber->dropped_msg_count = 0U;
}

msg_broker_status_e msg_subscriber_receive(msg_subscriber_t* inout_subscriber, msg_t* out_msg)
{
    ASSERT(inout_subscriber != NULL);
    ASSERT(out_msg != NULL);

    if (!msg_queue_pop(&inout_subscriber->queue, out_msg))
    {
        return MSG_BROKER_STATUS_IDLE;
    }

    return MSG_BROKER_STATUS_OK;
}

uint32_t msg_subscriber_get_dropped_msg_count(const msg_subscriber_t* in_subscriber)
{
    ASSERT(in_subscriber != NULL);

    return in_subscriber->dropped_msg_count;
}

/* ==========================================================================
 * msg_broker - public
 * ========================================================================= */

void msg_broker_init(msg_broker_t* inout_broker, msg_t* inout_msg_buffer, uint16_t in_capacity)
{
    ASSERT(inout_broker != NULL);

    /* Deliberately no "not already initialized" assertion, unlike the singleton this
     * was adapted from: on an instance that would read uninitialized memory, since a
     * caller may hand in a fresh stack or heap object. Init is the operation that makes
     * the state defined, so it cannot depend on it. */
    msg_queue_init(&inout_broker->input_queue, inout_msg_buffer, in_capacity);

    for (uint16_t topic = 0U; topic < MSG_LAST; ++topic)
    {
        for (uint16_t slot = 0U; slot < MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
        {
            inout_broker->topics[topic][slot] = NULL;
        }
    }

    inout_broker->published_msg_count = 0U;
    inout_broker->delivered_msg_count = 0U;
    inout_broker->dropped_msg_count = 0U;
    inout_broker->is_started = false;
    inout_broker->is_initialized = true;
}

void msg_broker_start(msg_broker_t* inout_broker)
{
    ASSERT(inout_broker != NULL);
    ASSERT(inout_broker->is_initialized);
    ASSERT(false == inout_broker->is_started);

    inout_broker->is_started = true;
}

void msg_broker_subscribe(msg_broker_t* inout_broker, msg_subscriber_t* inout_subscriber, msg_id_e in_topic)
{
    ASSERT(inout_broker != NULL);
    ASSERT(inout_broker->is_initialized);
    ASSERT(inout_subscriber != NULL);
    prv_assert_topic(in_topic);

    for (uint16_t slot = 0U; slot < MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        /* Subscribing twice would deliver twice, which no handler expects. */
        ASSERT(inout_broker->topics[in_topic][slot] != inout_subscriber);
    }

    for (uint16_t slot = 0U; slot < MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC; ++slot)
    {
        if (inout_broker->topics[in_topic][slot] == NULL)
        {
            inout_broker->topics[in_topic][slot] = inout_subscriber;

            return;
        }
    }

    /* Out of slots — raise MSG_BROKER_MAX_SUBSCRIBERS_PER_TOPIC. */
    ASSERT(false);
}

msg_broker_status_e msg_broker_publish(msg_broker_t* inout_broker, const msg_t* in_msg)
{
    prv_assert_started(inout_broker);
    ASSERT(in_msg != NULL);
    prv_assert_topic(in_msg->id);
    ASSERT(in_msg->payload_size <= MSG_PAYLOAD_MAX_SIZE);

    if (!msg_queue_push(&inout_broker->input_queue, in_msg))
    {
        /* Deliberately a status, not a block and not an assertion: a full queue is a
         * load condition the publisher is expected to handle (NFR-105). */
        return MSG_BROKER_STATUS_INPUT_FULL;
    }

    ++inout_broker->published_msg_count;

    return MSG_BROKER_STATUS_OK;
}

bool msg_broker_has_input_space(const msg_broker_t* in_broker, uint16_t in_headroom)
{
    ASSERT(in_broker != NULL);
    ASSERT(in_broker->is_initialized);

    return msg_queue_get_free_count(&in_broker->input_queue) >= in_headroom;
}

msg_broker_status_e msg_broker_process(msg_broker_t* inout_broker)
{
    msg_t msg;

    prv_assert_started(inout_broker);

    if (!msg_queue_pop(&inout_broker->input_queue, &msg))
    {
        return MSG_BROKER_STATUS_IDLE;
    }

    if (prv_copy_msg_to_subscribers(inout_broker, &msg) == 0U)
    {
        /* Not an error: a topic nobody has subscribed to yet is normal during bring-up,
         * and the reference implementation had this assertion commented out for the same
         * reason. Reported so a caller can log it if it cares. */
        return MSG_BROKER_STATUS_NO_SUBSCRIBER;
    }

    return MSG_BROKER_STATUS_OK;
}

uint32_t msg_broker_process_all(msg_broker_t* inout_broker)
{
    uint32_t moved_msg_count = 0U;

    prv_assert_started(inout_broker);

    while (msg_broker_process(inout_broker) != MSG_BROKER_STATUS_IDLE)
    {
        ++moved_msg_count;
    }

    return moved_msg_count;
}

uint32_t msg_broker_get_dropped_msg_count(const msg_broker_t* in_broker)
{
    ASSERT(in_broker != NULL);

    return in_broker->dropped_msg_count;
}
