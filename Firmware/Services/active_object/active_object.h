/*
 * active_object.h
 *
 * The Active Object template every module is built on (FR-109,
 * [03 §3.5](../../../Docu/PrePlanning/03-Architecture.md)). A module bundles its private
 * data, a single inbound message queue that is its only external input, and a handler
 * that reacts to messages. This is the boilerplate §3.5 calls for factoring out, so a
 * module contributes only its handler and its state.
 *
 * The pattern's four rules, and where each one lives:
 *
 * - **No shared data.** A module's state is reached only through the `context` pointer
 *   handed to its handler, and only while that handler runs. Nothing here exposes it.
 * - **Asynchronous messaging only.** A module is reached by publishing, never by a
 *   direct call. The broker's output queue makes that structural: a publisher never
 *   waits for a consumer.
 * - **Run-to-completion.** One message is handled fully before the next is taken. Not
 *   just documented — #active_object_process_one asserts on re-entry, so a handler that
 *   dispatches back into its own object is caught rather than corrupting state
 *   mid-update.
 * - **No blocking in handlers.** The only place a module waits is on its empty queue.
 *   Anything else that would block is modelled as a later message.
 *
 * **Threading.** §3.4 puts each module in its own task, with FreeRTOS arriving in M4. It
 * also says the host build runs these "as plain functions/loops driven by the SDL event
 * loop instead of tasks" — so polling #active_object_process_all is the host model, not a
 * stand-in for one. On the target the task body becomes the same call in a loop; nothing
 * in a module changes.
 */

#ifndef ACTIVE_OBJECT_H
#define ACTIVE_OBJECT_H

#include <stdbool.h>
#include <stdint.h>

#include "msg.h"
#include "msg_broker.h"

/* ==========================================================================
 * active_object - public types
 * ========================================================================= */

/*! \brief A module's message handler — its state machine's entry point.
 *
 * Must return before it can be called again (run-to-completion) and must not block.
 *
 * \param[in,out]   inout_context: the module's private data, as given to
 *                      #active_object_init
 * \param[in]       in_msg: the message to handle, borrowed for the call only
 */
typedef void (*active_object_dispatch_fn)(void* inout_context, const msg_t* in_msg);

typedef struct
{
    msg_subscriber_t subscriber;           /*!< The single inbound queue          */
    active_object_dispatch_fn dispatch_fn; /*!< The module's handler              */
    void* context;                         /*!< The module's private data         */
    const char* name;                      /*!< For diagnostics and logging       */
    bool is_initialized;
    bool is_dispatching; /*!< Run-to-completion guard           */
    uint32_t handled_msg_count;
} active_object_t;

/* ==========================================================================
 * active_object - public API
 * ========================================================================= */

/*! \brief Build an active object around a module's handler and state.
 *
 * \param[out]      inout_object: instance to initialize, must not be `NULL`
 * \param[in]       in_name: short name for diagnostics, must not be `NULL`. Borrowed —
 *                      pass a string literal.
 * \param[in]       inout_msg_buffer: array of at least `in_capacity` messages, borrowed
 *                      for the object's lifetime
 * \param[in]       in_capacity: inbound queue depth, at least `1`. Sizing matters: a
 *                      queue too shallow for the module's burst rate silently drops
 *                      messages, which #active_object_get_dropped_msg_count surfaces.
 * \param[in]       in_dispatch_fn: the module's handler, must not be `NULL`
 * \param[in,out]   inout_context: the module's private data, passed back to the
 *                      handler. May be `NULL` for a stateless module.
 */
void active_object_init(active_object_t* inout_object, const char* in_name, msg_t* inout_msg_buffer,
                        uint16_t in_capacity, active_object_dispatch_fn in_dispatch_fn, void* inout_context);

/*! \brief Register this object's inbound queue for one topic.
 *
 * The broker is passed in rather than assumed, because a module may sit on the system
 * broker or on the game's internal one (FR-110, §3.6).
 *
 * \param[in,out]   inout_object: initialized instance
 * \param[in,out]   inout_broker: the broker to subscribe on
 * \param[in]       in_topic: topic to receive
 */
void active_object_subscribe(active_object_t* inout_object, msg_broker_t* inout_broker, msg_id_e in_topic);

/*! \brief Handle at most one queued message, to completion.
 *
 * \param[in,out]   inout_object: initialized instance
 * \return          `true` when a message was handled, `false` when the queue was empty
 */
bool active_object_process_one(active_object_t* inout_object);

/*! \brief Handle every queued message, each to completion.
 *
 * This is the module's task body: on the host it is called from the event loop, on the
 * target it will be the body of the module's task.
 *
 * \param[in,out]   inout_object: initialized instance
 * \return          Number of messages handled
 */
uint32_t active_object_process_all(active_object_t* inout_object);

/*! \brief How many messages this object has handled since #active_object_init. */
uint32_t active_object_get_handled_msg_count(const active_object_t* in_object);

/*! \brief How many messages never reached this object because its queue was full.
 *
 * Non-zero means the module is not draining fast enough, or its queue is sized too
 * small. The broker drops for the slow subscriber rather than stalling everyone else, so
 * without checking this the loss is silent.
 */
uint32_t active_object_get_dropped_msg_count(const active_object_t* in_object);

/*! \brief The object's diagnostic name. */
const char* active_object_get_name(const active_object_t* in_object);

#endif /* ACTIVE_OBJECT_H */
