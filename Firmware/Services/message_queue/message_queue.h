/*
 * message_queue.h
 *
 * A fixed-capacity FIFO of messages, copied in and out by value. No heap: the caller
 * supplies the storage, so a module's queue lives in that module's own memory
 * (NFR-103).
 *
 * Its own module because the broker needs two of them with different owners — one
 * input queue it owns itself, and one output queue per subscriber — and because
 * wrap-around arithmetic is where this kind of code goes wrong.
 *
 * Not thread-safe on its own. Under FreeRTOS (M4) this is where the queue primitive
 * gets swapped for the RTOS's own, or a critical section gets added.
 */

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "message.h"

/* ==========================================================================
 * message_queue - public types
 * ========================================================================= */

typedef struct
{
    message_t* storage;                         /*!< Caller-owned array of `capacity` */
    uint16_t capacity;                          /*!< Number of messages it can hold   */
    uint16_t count;                             /*!< Number currently queued          */
    uint16_t read_index;                        /*!< Next slot to read                */
    uint16_t write_index;                       /*!< Next slot to write               */
} message_queue_t;

/* ==========================================================================
 * message_queue - public API
 * ========================================================================= */

/*! \brief Attach storage to a queue and empty it.
 *
 * \param[out]      inout_queue: queue to initialize, must not be `NULL`
 * \param[in]       inout_storage: array of at least `in_capacity` messages, must not
 *                      be `NULL`. Borrowed for the queue's lifetime.
 * \param[in]       in_capacity: number of messages, at least `1`
 */
void message_queue_init(message_queue_t* inout_queue, message_t* inout_storage,
                        uint16_t in_capacity);

/*! \brief Drop everything queued, keeping the storage. */
void message_queue_clear(message_queue_t* inout_queue);

/*! \brief Copy a message onto the back of the queue.
 *
 * \param[in,out]   inout_queue: initialized queue
 * \param[in]       in_message: message to copy in, must not be `NULL`
 * \return          `true` on success, `false` when the queue is full — the caller
 *                      decides whether that is backpressure or a dropped message
 */
bool message_queue_push(message_queue_t* inout_queue, const message_t* in_message);

/*! \brief Copy the message at the front of the queue out and remove it.
 *
 * \param[in,out]   inout_queue: initialized queue
 * \param[out]      out_message: receives the message, must not be `NULL`
 * \return          `true` when a message was taken, `false` when the queue was empty
 */
bool message_queue_pop(message_queue_t* inout_queue, message_t* out_message);

/*! \brief Number of messages currently queued. */
uint16_t message_queue_get_count(const message_queue_t* in_queue);

/*! \brief Number of further messages that would fit. */
uint16_t message_queue_get_free_count(const message_queue_t* in_queue);

/*! \brief Whether the queue holds nothing. */
bool message_queue_is_empty(const message_queue_t* in_queue);

/*! \brief Whether the queue can take no more. */
bool message_queue_is_full(const message_queue_t* in_queue);

#endif /* MESSAGE_QUEUE_H */
