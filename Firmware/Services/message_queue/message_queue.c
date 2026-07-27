#include "message_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"
#include "message.h"

/* ==========================================================================
 * message_queue - private
 * ========================================================================= */

/* Advance a ring index, wrapping at the capacity. */
static uint16_t prv_next_index(uint16_t in_index, uint16_t in_capacity)
{
    const uint16_t next = (uint16_t)(in_index + 1U);

    return (next == in_capacity) ? 0U : next;
}

static void prv_assert_initialized(const message_queue_t* const in_queue)
{
    ASSERT(in_queue != NULL);
    ASSERT(in_queue->storage != NULL);
    ASSERT(in_queue->capacity > 0U);
}

/* ==========================================================================
 * message_queue - public
 * ========================================================================= */

void message_queue_init(message_queue_t* inout_queue, message_t* inout_storage,
                        uint16_t in_capacity)
{
    ASSERT(inout_queue != NULL);
    ASSERT(inout_storage != NULL);
    ASSERT(in_capacity > 0U);

    inout_queue->storage = inout_storage;
    inout_queue->capacity = in_capacity;

    message_queue_clear(inout_queue);
}

void message_queue_clear(message_queue_t* inout_queue)
{
    prv_assert_initialized(inout_queue);

    inout_queue->count = 0U;
    inout_queue->read_index = 0U;
    inout_queue->write_index = 0U;
}

bool message_queue_push(message_queue_t* inout_queue, const message_t* in_message)
{
    prv_assert_initialized(inout_queue);
    ASSERT(in_message != NULL);

    if (inout_queue->count == inout_queue->capacity)
    {
        return false;
    }

    memcpy(&inout_queue->storage[inout_queue->write_index], in_message, sizeof(*in_message));

    inout_queue->write_index = prv_next_index(inout_queue->write_index, inout_queue->capacity);
    ++inout_queue->count;

    return true;
}

bool message_queue_pop(message_queue_t* inout_queue, message_t* out_message)
{
    prv_assert_initialized(inout_queue);
    ASSERT(out_message != NULL);

    if (inout_queue->count == 0U)
    {
        return false;
    }

    memcpy(out_message, &inout_queue->storage[inout_queue->read_index], sizeof(*out_message));

    inout_queue->read_index = prv_next_index(inout_queue->read_index, inout_queue->capacity);
    --inout_queue->count;

    return true;
}

uint16_t message_queue_get_count(const message_queue_t* in_queue)
{
    prv_assert_initialized(in_queue);

    return in_queue->count;
}

uint16_t message_queue_get_free_count(const message_queue_t* in_queue)
{
    prv_assert_initialized(in_queue);

    return (uint16_t)(in_queue->capacity - in_queue->count);
}

bool message_queue_is_empty(const message_queue_t* in_queue)
{
    return message_queue_get_count(in_queue) == 0U;
}

bool message_queue_is_full(const message_queue_t* in_queue)
{
    return message_queue_get_free_count(in_queue) == 0U;
}
