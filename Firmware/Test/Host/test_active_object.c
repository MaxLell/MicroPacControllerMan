/*
 * Unit tests for Services/active_object.
 *
 * Two of these earn their keep well beyond the plumbing, because they check the Active
 * Object *rules* from 03 §3.5 rather than the code that implements them:
 *
 * - **Run-to-completion.** A handler that dispatches back into its own object is caught.
 *   Without that, a second message would mutate the module's state halfway through the
 *   first update — the exact bug class the pattern exists to remove, and one that in a
 *   running game would look like a random glitch.
 * - **Asynchronous only.** A handler that publishes must not deliver synchronously. If
 *   it did, the publisher would be running inside the consumer and the "no shared data"
 *   guarantee would be void.
 *
 * Everything is on the real broker: the point is the interaction between the two, so
 * mocking the broker would test nothing worth knowing.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "active_object.h"
#include "assert_probe.h"
/* Ceedling resolves sources from this file's includes only, not transitively — the
 * broker sits three modules above the ring buffer. */
#include "circular_buffer.h"
#include "custom_assert.h"
#include "msg.h"
#include "msg_broker.h"
#include "msg_queue.h"
#include "unity.h"

#define TEST_INPUT_CAPACITY (8U)
#define TEST_INBOX_CAPACITY (4U)
#define TEST_SMALL_INBOX_CAPACITY (1U)

#define TEST_SCORE_FIRST (11U)
#define TEST_SCORE_SECOND (22U)
#define TEST_SCORE_THIRD (33U)

#define TEST_NO_MSGS (0U)
#define TEST_ONE_MSG (1U)
#define TEST_TWO_MSGS (2U)
#define TEST_ONE_DROP (1U)

#define TEST_FIRST_OBJECT_NAME "first"
#define TEST_SECOND_OBJECT_NAME "second"

/*! \brief Stand-in for a module's private state. */
typedef struct
{
    uint32_t handler_call_count;
    uint32_t last_score;
    msg_id_e last_topic;
} test_module_state_t;

static msg_t g_broker_input_msg_buffer[TEST_INPUT_CAPACITY];
static msg_t g_first_inbox_msg_buffer[TEST_INBOX_CAPACITY];
static msg_t g_second_inbox_msg_buffer[TEST_INBOX_CAPACITY];

static msg_broker_t g_broker;
static active_object_t g_first_object;
static active_object_t g_second_object;
static test_module_state_t g_first_state;
static test_module_state_t g_second_state;

/* Set when a handler wants to try something the pattern forbids. */
static bool g_handler_shall_reenter;
static bool g_handler_shall_publish;

static msg_t prv_make_score_msg(msg_id_e in_topic, uint32_t in_score)
{
    msg_t msg = {0};
    msg_game_score_t payload = {0};

    payload.score = in_score;

    msg.id = in_topic;
    msg.payload_size = (uint16_t)sizeof(payload);
    memcpy(msg.payload, &payload, sizeof(payload));

    return msg;
}

static uint32_t prv_score_of(const msg_t* const in_msg)
{
    msg_game_score_t payload = {0};

    memcpy(&payload, in_msg->payload, sizeof(payload));

    return payload.score;
}

static void prv_publish_score(msg_id_e in_topic, uint32_t in_score)
{
    const msg_t msg = prv_make_score_msg(in_topic, in_score);

    TEST_ASSERT_EQUAL(MSG_BROKER_STATUS_OK, msg_broker_publish(&g_broker, &msg));
}

/* A module handler: records what it was given, and on request misbehaves. */
static void prv_handle_msg(void* inout_context, const msg_t* in_msg)
{
    test_module_state_t* const state = inout_context;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(in_msg);

    ++state->handler_call_count;
    state->last_topic = in_msg->id;
    state->last_score = prv_score_of(in_msg);

    if (g_handler_shall_publish)
    {
        g_handler_shall_publish = false;
        prv_publish_score(MSG_GAME_OVER, TEST_SCORE_THIRD);
    }

    if (g_handler_shall_reenter)
    {
        g_handler_shall_reenter = false;
        /* Forbidden by run-to-completion — must be caught, not obeyed. */
        (void)active_object_process_one(&g_first_object);
    }
}

void setUp(void)
{
    assert_probe_begin();

    memset(&g_broker, 0, sizeof(g_broker));
    memset(&g_first_state, 0, sizeof(g_first_state));
    memset(&g_second_state, 0, sizeof(g_second_state));

    g_handler_shall_reenter = false;
    g_handler_shall_publish = false;

    msg_broker_init(&g_broker, g_broker_input_msg_buffer, TEST_INPUT_CAPACITY);

    active_object_init(&g_first_object, TEST_FIRST_OBJECT_NAME, g_first_inbox_msg_buffer,
                       TEST_INBOX_CAPACITY, prv_handle_msg, &g_first_state);
    active_object_init(&g_second_object, TEST_SECOND_OBJECT_NAME, g_second_inbox_msg_buffer,
                       TEST_INBOX_CAPACITY, prv_handle_msg, &g_second_state);
}

void tearDown(void)
{
    assert_probe_end();
}

/* --- the basics ----------------------------------------------------------- */

void test_a_new_object_has_handled_nothing(void)
{
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, active_object_get_handled_msg_count(&g_first_object));
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, active_object_get_dropped_msg_count(&g_first_object));
    TEST_ASSERT_EQUAL_STRING(TEST_FIRST_OBJECT_NAME, active_object_get_name(&g_first_object));
}

void test_processing_an_empty_inbox_handles_nothing(void)
{
    TEST_ASSERT_FALSE(active_object_process_one(&g_first_object));
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, g_first_state.handler_call_count);
}

void test_a_subscribed_object_receives_its_topic(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_TRUE(active_object_process_one(&g_first_object));
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, g_first_state.handler_call_count);
    TEST_ASSERT_EQUAL(MSG_GAME_SCORE_UPDATED, g_first_state.last_topic);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, g_first_state.last_score);
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, active_object_get_handled_msg_count(&g_first_object));
}

void test_the_handler_gets_its_own_context(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    active_object_subscribe(&g_second_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    (void)msg_broker_process_all(&g_broker);

    (void)active_object_process_one(&g_first_object);

    /* Same handler function, different state: nothing is shared between modules. */
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, g_first_state.handler_call_count);
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, g_second_state.handler_call_count);
}

void test_one_object_can_receive_several_topics(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_OVER);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_OVER, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(TEST_TWO_MSGS, active_object_process_all(&g_first_object));
    TEST_ASSERT_EQUAL(MSG_GAME_OVER, g_first_state.last_topic);
}

void test_process_one_takes_exactly_one_message(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_TRUE(active_object_process_one(&g_first_object));

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, g_first_state.handler_call_count);
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_FIRST, g_first_state.last_score);
}

void test_process_all_drains_the_inbox_in_order(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(TEST_TWO_MSGS, active_object_process_all(&g_first_object));

    /* The last one handled is the last one published. */
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_SECOND, g_first_state.last_score);
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, active_object_process_all(&g_first_object));
}

/* --- run-to-completion (§3.5) --------------------------------------------- */

void test_a_handler_that_dispatches_into_its_own_object_is_caught(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    g_handler_shall_reenter = true;

    /* The nested call must trip the guard rather than start handling the second message
     * while the first is still in flight. */
    ASSERT_PROBE_EXPECT(active_object_process_one(&g_first_object),
                        "false == inout_object->is_dispatching");
}

/* --- asynchronous messaging only (§3.5) ----------------------------------- */

void test_publishing_from_a_handler_does_not_deliver_synchronously(void)
{
    active_object_subscribe(&g_first_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    active_object_subscribe(&g_second_object, &g_broker, MSG_GAME_OVER);
    msg_broker_start(&g_broker);

    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    (void)msg_broker_process_all(&g_broker);

    g_handler_shall_publish = true;
    (void)active_object_process_one(&g_first_object);

    /* The first handler published to the second object's topic. If that had been
     * delivered synchronously, the second handler would have run inside the first — and
     * "no shared data" would mean nothing. */
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, g_second_state.handler_call_count);
    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, active_object_process_all(&g_second_object));

    /* It arrives once the broker moves it, on the next turn of the loop. */
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, active_object_process_all(&g_second_object));
    TEST_ASSERT_EQUAL_UINT32(TEST_SCORE_THIRD, g_second_state.last_score);
}

/* --- a module that cannot keep up ----------------------------------------- */

void test_an_object_with_a_full_inbox_reports_the_loss(void)
{
    static msg_t small_inbox_msg_buffer[TEST_SMALL_INBOX_CAPACITY];
    static test_module_state_t slow_state;
    active_object_t slow_object;

    memset(&slow_state, 0, sizeof(slow_state));

    active_object_init(&slow_object, "slow", small_inbox_msg_buffer, TEST_SMALL_INBOX_CAPACITY,
                       prv_handle_msg, &slow_state);
    active_object_subscribe(&slow_object, &g_broker, MSG_GAME_SCORE_UPDATED);
    msg_broker_start(&g_broker);

    /* Two messages into a one-deep inbox that nobody drains in between. */
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_FIRST);
    prv_publish_score(MSG_GAME_SCORE_UPDATED, TEST_SCORE_SECOND);
    (void)msg_broker_process_all(&g_broker);

    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_DROP, active_object_get_dropped_msg_count(&slow_object));
    /* The one that fit is still handled — a full inbox costs a message, not the module. */
    TEST_ASSERT_EQUAL_UINT32(TEST_ONE_MSG, active_object_process_all(&slow_object));
}

/* --- preconditions -------------------------------------------------------- */

void test_an_object_without_a_handler_asserts(void)
{
    active_object_t bad_object;

    ASSERT_PROBE_EXPECT(active_object_init(&bad_object, TEST_FIRST_OBJECT_NAME,
                                          g_first_inbox_msg_buffer, TEST_INBOX_CAPACITY, NULL,
                                          &g_first_state),
                        "in_dispatch_fn != NULL");
}

void test_an_object_without_a_name_asserts(void)
{
    active_object_t bad_object;

    ASSERT_PROBE_EXPECT(active_object_init(&bad_object, NULL, g_first_inbox_msg_buffer,
                                          TEST_INBOX_CAPACITY, prv_handle_msg, &g_first_state),
                        "in_name != NULL");
}

void test_a_stateless_module_may_pass_no_context(void)
{
    static msg_t stateless_inbox_msg_buffer[TEST_INBOX_CAPACITY];
    active_object_t stateless_object;

    /* A module with nothing to remember is legitimate — the path-planning library of
     * §3.6 is one — so a NULL context must be accepted. */
    active_object_init(&stateless_object, "stateless", stateless_inbox_msg_buffer,
                       TEST_INBOX_CAPACITY, prv_handle_msg, NULL);

    TEST_ASSERT_EQUAL_UINT32(TEST_NO_MSGS, active_object_get_handled_msg_count(&stateless_object));
}
