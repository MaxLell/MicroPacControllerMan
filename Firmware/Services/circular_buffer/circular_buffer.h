/*
 * circular_buffer.h
 *
 * A fixed-capacity FIFO ring buffer of same-sized elements, copied in and out by value.
 * Element-type-agnostic: it moves `element_size` bytes and never looks at them.
 *
 * No heap — the caller supplies the storage, so a buffer lives in the memory of
 * whoever owns it (NFR-103).
 *
 * A component in its own right rather than something hidden inside one user: the ring
 * arithmetic is identical whatever is queued, and it is where off-by-one and
 * wrap-around bugs live, so it is worth having one tested copy. #msg_queue_t is the
 * first user; a byte or event queue would use the same thing.
 *
 * Not thread-safe on its own, and the firmware needs no thread safety here: everything
 * runs in one cooperative loop (§3.4, DEC-027). Where a producer *is* an interrupt — the
 * console's receive path — the buffer is not this one, because `count` is shared between
 * producer and consumer; see `console.c`.
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * circular_buffer - public types
 * ========================================================================= */

typedef struct
{
    uint8_t* storage;     /*!< Caller-owned, capacity elements   */
    size_t element_size;  /*!< Bytes per element                 */
    uint16_t capacity;    /*!< Elements it can hold              */
    uint16_t count;       /*!< Elements currently held           */
    uint16_t read_index;  /*!< Next element to read              */
    uint16_t write_index; /*!< Next slot to write                */
} circular_buffer_t;

/* ==========================================================================
 * circular_buffer - public API
 * ========================================================================= */

/*! \brief Attach storage to a buffer and empty it.
 *
 * \param[out]      inout_buffer: buffer to initialize, must not be `NULL`
 * \param[in]       inout_storage: at least `in_capacity * in_element_size` bytes,
 *                      must not be `NULL`. Borrowed for the buffer's lifetime.
 * \param[in]       in_element_size: bytes per element, at least `1`
 * \param[in]       in_capacity: number of elements, at least `1`
 */
void circular_buffer_init(circular_buffer_t* inout_buffer, void* inout_storage, size_t in_element_size,
                          uint16_t in_capacity);

/*! \brief Drop everything held, keeping the storage. */
void circular_buffer_clear(circular_buffer_t* inout_buffer);

/*! \brief Copy an element onto the back of the buffer.
 *
 * \param[in,out]   inout_buffer: initialized buffer
 * \param[in]       in_element: `element_size` bytes to copy in, must not be `NULL`
 * \return          `true` on success, `false` when the buffer is full. The caller
 *                      decides whether a full buffer is backpressure or a drop — this
 *                      component never overwrites unread data.
 */
bool circular_buffer_push(circular_buffer_t* inout_buffer, const void* in_element);

/*! \brief Copy the element at the front of the buffer out and remove it.
 *
 * \param[in,out]   inout_buffer: initialized buffer
 * \param[out]      out_element: receives `element_size` bytes, must not be `NULL`
 * \return          `true` when an element was taken, `false` when the buffer was empty
 */
bool circular_buffer_pop(circular_buffer_t* inout_buffer, void* out_element);

/*! \brief Number of elements currently held. */
uint16_t circular_buffer_get_count(const circular_buffer_t* in_buffer);

/*! \brief Number of further elements that would fit. */
uint16_t circular_buffer_get_free_count(const circular_buffer_t* in_buffer);

/*! \brief Whether the buffer holds nothing. */
bool circular_buffer_is_empty(const circular_buffer_t* in_buffer);

/*! \brief Whether the buffer can take no more. */
bool circular_buffer_is_full(const circular_buffer_t* in_buffer);

#endif /* CIRCULAR_BUFFER_H */
