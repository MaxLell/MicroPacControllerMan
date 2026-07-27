/*
 * msg_queue.h
 *
 * A FIFO of messages: a type-safe skin over #circular_buffer_t, which does the ring
 * arithmetic. This module exists so callers pass and receive `msg_t` instead of `void*`,
 * and so the element size cannot be got wrong at a call site.
 *
 * No heap — the caller supplies the buffer, so a module's queue lives in that module's
 * own memory (NFR-103).
 */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "circular_buffer.h"
#include "msg.h"

/* ==========================================================================
 * msg_queue - public types
 * ========================================================================= */

typedef struct
{
    circular_buffer_t buffer;
} msg_queue_t;

/* ==========================================================================
 * msg_queue - public API
 * ========================================================================= */

/*! \brief Attach a message buffer to a queue and empty it.
 *
 * \param[out]      inout_queue: queue to initialize, must not be `NULL`
 * \param[in]       inout_msg_buffer: array of at least `in_capacity` messages, must
 *                      not be `NULL`. Borrowed for the queue's lifetime.
 * \param[in]       in_capacity: number of messages, at least `1`
 */
void msg_queue_init(msg_queue_t* inout_queue, msg_t* inout_msg_buffer, uint16_t in_capacity);

/*! \brief Drop every queued message, keeping the buffer. */
void msg_queue_clear(msg_queue_t* inout_queue);

/*! \brief Copy a message onto the back of the queue.
 *
 * \param[in,out]   inout_queue: initialized queue
 * \param[in]       in_msg: message to copy in, must not be `NULL`
 * \return          `true` on success, `false` when the queue is full — the caller
 *                      decides whether that is backpressure or a dropped message
 */
bool msg_queue_push(msg_queue_t* inout_queue, const msg_t* in_msg);

/*! \brief Copy the message at the front of the queue out and remove it.
 *
 * \param[in,out]   inout_queue: initialized queue
 * \param[out]      out_msg: receives the message, must not be `NULL`
 * \return          `true` when a message was taken, `false` when the queue was empty
 */
bool msg_queue_pop(msg_queue_t* inout_queue, msg_t* out_msg);

/*! \brief Number of messages currently queued. */
uint16_t msg_queue_get_count(const msg_queue_t* in_queue);

/*! \brief Number of further messages that would fit. */
uint16_t msg_queue_get_free_count(const msg_queue_t* in_queue);

/*! \brief Whether the queue holds no messages. */
bool msg_queue_is_empty(const msg_queue_t* in_queue);

/*! \brief Whether the queue can take no more messages. */
bool msg_queue_is_full(const msg_queue_t* in_queue);

#endif /* MSG_QUEUE_H */
