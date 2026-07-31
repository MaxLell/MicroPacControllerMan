#include "active_object.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "custom_assert.h"
#include "msg.h"
#include "msg_broker.h"

/* ==========================================================================
 * active_object - private
 * ========================================================================= */

static void prv_assert_initialized(const active_object_t* const in_object)
{
    ASSERT(in_object != NULL);
    ASSERT(in_object->is_initialized);
    ASSERT(in_object->dispatch_fn != NULL);
}

/* ==========================================================================
 * active_object - public
 * ========================================================================= */

void active_object_init(active_object_t* inout_object, const char* in_name, msg_t* inout_msg_buffer,
                        uint16_t in_capacity, active_object_dispatch_fn in_dispatch_fn, void* inout_context)
{
    ASSERT(inout_object != NULL);
    ASSERT(in_name != NULL);
    ASSERT(in_dispatch_fn != NULL);

    msg_subscriber_init(&inout_object->subscriber, inout_msg_buffer, in_capacity);

    inout_object->dispatch_fn = in_dispatch_fn;
    inout_object->context = inout_context;
    inout_object->name = in_name;
    inout_object->handled_msg_count = 0U;
    inout_object->is_dispatching = false;
    inout_object->is_initialized = true;
}

void active_object_subscribe(active_object_t* inout_object, msg_broker_t* inout_broker, msg_id_e in_topic)
{
    prv_assert_initialized(inout_object);

    msg_broker_subscribe(inout_broker, &inout_object->subscriber, in_topic);
}

bool active_object_process_one(active_object_t* inout_object)
{
    msg_t msg;

    prv_assert_initialized(inout_object);

    /* Run-to-completion (§3.5). Getting here while a handler is still running means that
     * handler dispatched back into its own object — directly, or through a call chain.
     * Allowing it would let a second message mutate the module's state halfway through
     * the first update, which is exactly the class of bug the pattern exists to remove.
     * So it is an assertion, not a silent re-entry. */
    ASSERT(false == inout_object->is_dispatching);

    if (msg_subscriber_receive(&inout_object->subscriber, &msg) != MSG_BROKER_STATUS_OK)
    {
        return false;
    }

    inout_object->is_dispatching = true;

    inout_object->dispatch_fn(inout_object->context, &msg);

    inout_object->is_dispatching = false;

    ++inout_object->handled_msg_count;

    return true;
}

uint32_t active_object_process_all(active_object_t* inout_object)
{
    uint32_t handled_count = 0U;

    prv_assert_initialized(inout_object);

    while (active_object_process_one(inout_object))
    {
        ++handled_count;
    }

    return handled_count;
}

uint32_t active_object_get_handled_msg_count(const active_object_t* in_object)
{
    prv_assert_initialized(in_object);

    return in_object->handled_msg_count;
}

uint32_t active_object_get_dropped_msg_count(const active_object_t* in_object)
{
    prv_assert_initialized(in_object);

    return msg_subscriber_get_dropped_msg_count(&in_object->subscriber);
}

const char* active_object_get_name(const active_object_t* in_object)
{
    prv_assert_initialized(in_object);

    return in_object->name;
}
