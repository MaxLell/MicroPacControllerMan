#include "msg_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "circular_buffer.h"
#include "custom_assert.h"
#include "msg.h"

/* ==========================================================================
 * msg_queue - private
 * ========================================================================= */

static void prv_assert_queue(const msg_queue_t* const in_queue)
{
    ASSERT(in_queue != NULL);
}

/* ==========================================================================
 * msg_queue - public
 * ========================================================================= */

void msg_queue_init(msg_queue_t* inout_queue, msg_t* inout_msg_buffer, uint16_t in_capacity)
{
    prv_assert_queue(inout_queue);

    circular_buffer_init(&inout_queue->buffer, inout_msg_buffer, sizeof(msg_t), in_capacity);
}

void msg_queue_clear(msg_queue_t* inout_queue)
{
    prv_assert_queue(inout_queue);

    circular_buffer_clear(&inout_queue->buffer);
}

bool msg_queue_push(msg_queue_t* inout_queue, const msg_t* in_msg)
{
    prv_assert_queue(inout_queue);

    return circular_buffer_push(&inout_queue->buffer, in_msg);
}

bool msg_queue_pop(msg_queue_t* inout_queue, msg_t* out_msg)
{
    prv_assert_queue(inout_queue);

    return circular_buffer_pop(&inout_queue->buffer, out_msg);
}

uint16_t msg_queue_get_count(const msg_queue_t* in_queue)
{
    prv_assert_queue(in_queue);

    return circular_buffer_get_count(&in_queue->buffer);
}

uint16_t msg_queue_get_free_count(const msg_queue_t* in_queue)
{
    prv_assert_queue(in_queue);

    return circular_buffer_get_free_count(&in_queue->buffer);
}

bool msg_queue_is_empty(const msg_queue_t* in_queue)
{
    prv_assert_queue(in_queue);

    return circular_buffer_is_empty(&in_queue->buffer);
}

bool msg_queue_is_full(const msg_queue_t* in_queue)
{
    prv_assert_queue(in_queue);

    return circular_buffer_is_full(&in_queue->buffer);
}
