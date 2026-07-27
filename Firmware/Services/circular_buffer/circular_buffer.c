#include "circular_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "custom_assert.h"

/* ==========================================================================
 * circular_buffer - private
 * ========================================================================= */

/* Advance a ring index, wrapping at the capacity. */
static uint16_t prv_advance_index(uint16_t in_index, uint16_t in_capacity)
{
    const uint16_t next = (uint16_t)(in_index + 1U);

    return (next == in_capacity) ? 0U : next;
}

static uint8_t* prv_slot_address(const circular_buffer_t* const in_buffer, uint16_t in_index)
{
    return &in_buffer->storage[(size_t)in_index * in_buffer->element_size];
}

static void prv_assert_initialized(const circular_buffer_t* const in_buffer)
{
    ASSERT(in_buffer != NULL);
    ASSERT(in_buffer->storage != NULL);
    ASSERT(in_buffer->element_size > 0U);
    ASSERT(in_buffer->capacity > 0U);
}

/* ==========================================================================
 * circular_buffer - public
 * ========================================================================= */

void circular_buffer_init(circular_buffer_t* inout_buffer, void* inout_storage,
                          size_t in_element_size, uint16_t in_capacity)
{
    ASSERT(inout_buffer != NULL);
    ASSERT(inout_storage != NULL);
    ASSERT(in_element_size > 0U);
    ASSERT(in_capacity > 0U);

    inout_buffer->storage = inout_storage;
    inout_buffer->element_size = in_element_size;
    inout_buffer->capacity = in_capacity;

    circular_buffer_clear(inout_buffer);
}

void circular_buffer_clear(circular_buffer_t* inout_buffer)
{
    prv_assert_initialized(inout_buffer);

    inout_buffer->count = 0U;
    inout_buffer->read_index = 0U;
    inout_buffer->write_index = 0U;
}

bool circular_buffer_push(circular_buffer_t* inout_buffer, const void* in_element)
{
    prv_assert_initialized(inout_buffer);
    ASSERT(in_element != NULL);

    if (inout_buffer->count == inout_buffer->capacity)
    {
        return false;
    }

    memcpy(prv_slot_address(inout_buffer, inout_buffer->write_index), in_element,
           inout_buffer->element_size);

    inout_buffer->write_index = prv_advance_index(inout_buffer->write_index, inout_buffer->capacity);
    ++inout_buffer->count;

    return true;
}

bool circular_buffer_pop(circular_buffer_t* inout_buffer, void* out_element)
{
    prv_assert_initialized(inout_buffer);
    ASSERT(out_element != NULL);

    if (inout_buffer->count == 0U)
    {
        return false;
    }

    memcpy(out_element, prv_slot_address(inout_buffer, inout_buffer->read_index),
           inout_buffer->element_size);

    inout_buffer->read_index = prv_advance_index(inout_buffer->read_index, inout_buffer->capacity);
    --inout_buffer->count;

    return true;
}

uint16_t circular_buffer_get_count(const circular_buffer_t* in_buffer)
{
    prv_assert_initialized(in_buffer);

    return in_buffer->count;
}

uint16_t circular_buffer_get_free_count(const circular_buffer_t* in_buffer)
{
    prv_assert_initialized(in_buffer);

    return (uint16_t)(in_buffer->capacity - in_buffer->count);
}

bool circular_buffer_is_empty(const circular_buffer_t* in_buffer)
{
    return circular_buffer_get_count(in_buffer) == 0U;
}

bool circular_buffer_is_full(const circular_buffer_t* in_buffer)
{
    return circular_buffer_get_free_count(in_buffer) == 0U;
}
