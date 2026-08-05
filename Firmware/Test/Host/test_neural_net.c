/*
 * Unit tests for Services/neural_net (VT-UNIT-011).
 *
 * The arithmetic is checked against sums worked out by hand rather than against a second
 * implementation, because a second implementation of "multiply and add" would be the same code
 * twice and would agree with a mistake.
 *
 * The tests that earn their place are the ones about the *contract* FR-039 needs: evaluation
 * holds no state between calls, and a table whose connections are numbered out of order is
 * rejected instead of quietly reading an uninitialised value. That last one is the failure this
 * module is built to make impossible — it would not crash, it would just play badly, and nothing
 * else in the system would notice.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assert_probe.h"
#include "custom_assert.h"
#include "neural_net.h"
#include "unity.h"

/* --- a network small enough to compute by hand ---------------------------- */
/*
 *   in0 --0.5--> h2 --1.0--> out3
 *   in1 --2.0-->    \-0.5--> out4
 *                       bias(h2) = -0.25, bias(out3) = 0.125, bias(out4) = 0
 */
#define INPUT_COUNT  (2U)
#define NODE_COUNT   (5U)
#define OUTPUT_COUNT (2U)

static const uint16_t g_output_nodes[OUTPUT_COUNT] = {3U, 4U};
static const float g_biases[NODE_COUNT] = {0.0F, 0.0F, -0.25F, 0.125F, 0.0F};
static const uint16_t g_offsets[NODE_COUNT + 1U] = {0U, 0U, 0U, 2U, 3U, 4U};
static const uint16_t g_sources[4] = {0U, 1U, 2U, 2U};
static const float g_weights[4] = {0.5F, 2.0F, 1.0F, -0.5F};

static neural_net_t g_net;
static float g_outputs[OUTPUT_COUNT];

void setUp(void)
{
    assert_probe_begin();

    g_net.input_count = INPUT_COUNT;
    g_net.node_count = NODE_COUNT;
    g_net.output_count = OUTPUT_COUNT;
    g_net.output_nodes = g_output_nodes;
    g_net.biases = g_biases;
    g_net.connection_offsets = g_offsets;
    g_net.connection_sources = g_sources;
    g_net.connection_weights = g_weights;

    memset(g_outputs, 0, sizeof(g_outputs));
}

void tearDown(void)
{
}

/* --- the arithmetic ------------------------------------------------------- */

void test_the_network_computes_the_sum_worked_out_by_hand(void)
{
    const float inputs[INPUT_COUNT] = {1.0F, 0.5F};

    /* h2   = relu(-0.25 + 0.5*1.0 + 2.0*0.5) = relu(1.25)  = 1.25
     * out3 = relu(0.125 + 1.0*1.25)          = relu(1.375) = 1.375
     * out4 = relu(0.0   - 0.5*1.25)          = relu(-0.625) = 0 */
    neural_net_evaluate(&g_net, inputs, g_outputs);

    TEST_ASSERT_EQUAL_FLOAT(1.375F, g_outputs[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, g_outputs[1]);
}

void test_relu_clamps_at_zero_rather_than_passing_a_negative_through(void)
{
    /* Both inputs negative drives the hidden node below zero, and everything after it must see
     * exactly 0 — not a small negative that would leak through the second layer. */
    const float inputs[INPUT_COUNT] = {-1.0F, -1.0F};

    neural_net_evaluate(&g_net, inputs, g_outputs);

    /* h2 = relu(-0.25 - 0.5 - 2.0) = 0, so out3 = relu(0.125) and out4 = relu(0). */
    TEST_ASSERT_EQUAL_FLOAT(0.125F, g_outputs[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, g_outputs[1]);
}

void test_a_node_with_no_connections_is_just_its_bias(void)
{
    /* Node 3 fed by nothing: an isolated output is a legitimate NEAT genome, and it must come
     * out as the bias rather than as whatever the scratch buffer held. */
    static const uint16_t offsets[NODE_COUNT + 1U] = {0U, 0U, 0U, 0U, 0U, 0U};
    const float inputs[INPUT_COUNT] = {5.0F, 5.0F};

    g_net.connection_offsets = offsets;

    neural_net_evaluate(&g_net, inputs, g_outputs);

    TEST_ASSERT_EQUAL_FLOAT(0.125F, g_outputs[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, g_outputs[1]);
}

/* --- the contract FR-039 depends on -------------------------------------- */

void test_evaluation_keeps_no_state_between_calls(void)
{
    const float first[INPUT_COUNT] = {1.0F, 0.5F};
    const float second[INPUT_COUNT] = {-1.0F, -1.0F};
    float first_outputs[OUTPUT_COUNT];

    neural_net_evaluate(&g_net, first, first_outputs);
    neural_net_evaluate(&g_net, second, g_outputs);
    neural_net_evaluate(&g_net, first, g_outputs);

    /* The same inputs after a different evaluation must give the same answer. The scratch buffer
     * is shared between calls, so this is what says the sharing is invisible. */
    TEST_ASSERT_EQUAL_FLOAT(first_outputs[0], g_outputs[0]);
    TEST_ASSERT_EQUAL_FLOAT(first_outputs[1], g_outputs[1]);
}

void test_repeated_evaluation_is_bit_identical(void)
{
    const float inputs[INPUT_COUNT] = {0.3F, 0.7F};
    float reference[OUTPUT_COUNT];

    neural_net_evaluate(&g_net, inputs, reference);

    for (uint8_t attempt = 0U; attempt < 8U; ++attempt)
    {
        neural_net_evaluate(&g_net, inputs, g_outputs);

        /* Not "within a tolerance": FR-039 compares an argmax across two machines, and a value
         * that wobbles between calls on *one* machine would make that comparison meaningless. */
        TEST_ASSERT_EQUAL_MEMORY(reference, g_outputs, sizeof(reference));
    }
}

/* --- a broken table is rejected, not evaluated --------------------------- */

void test_a_well_formed_network_is_accepted(void)
{
    TEST_ASSERT_TRUE(neural_net_is_well_formed(&g_net));
}

void test_a_connection_from_a_later_node_is_rejected(void)
{
    /* The invariant the evaluation rests on. Node 3 fed from node 4, which has not been computed
     * when node 3 is: the answer would be whatever the buffer held from the previous call —
     * plausible, wrong, and invisible to every other test in this suite. */
    static const uint16_t sources[4] = {0U, 1U, 4U, 2U};

    g_net.connection_sources = sources;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_a_self_connection_is_rejected(void)
{
    static const uint16_t sources[4] = {0U, 1U, 3U, 2U};

    g_net.connection_sources = sources;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_a_connection_into_an_input_is_rejected(void)
{
    /* An input's value is handed in, so a connection feeding one would be dropped without
     * complaint and the evaluated network would not be the trained one. */
    static const uint16_t offsets[NODE_COUNT + 1U] = {0U, 1U, 1U, 3U, 4U, 4U};

    g_net.connection_offsets = offsets;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_descending_offsets_are_rejected(void)
{
    static const uint16_t offsets[NODE_COUNT + 1U] = {0U, 0U, 0U, 2U, 1U, 4U};

    g_net.connection_offsets = offsets;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_an_output_node_out_of_range_is_rejected(void)
{
    static const uint16_t output_nodes[OUTPUT_COUNT] = {3U, NODE_COUNT};

    g_net.output_nodes = output_nodes;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_a_network_too_large_for_the_scratch_buffer_is_rejected(void)
{
    g_net.node_count = NEURAL_NET_MAX_NODES + 1U;

    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

void test_an_empty_or_pointerless_network_is_rejected(void)
{
    g_net.node_count = 0U;
    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));

    setUp();
    g_net.output_count = 0U;
    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));

    setUp();
    g_net.biases = NULL;
    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));

    setUp();
    g_net.input_count = (uint16_t)(NODE_COUNT + 1U);
    TEST_ASSERT_FALSE(neural_net_is_well_formed(&g_net));
}

/* --- preconditions ------------------------------------------------------- */

void test_a_null_argument_asserts(void)
{
    const float inputs[INPUT_COUNT] = {0.0F, 0.0F};

    ASSERT_PROBE_EXPECT((void)neural_net_is_well_formed(NULL), "in_net != NULL");
    ASSERT_PROBE_EXPECT(neural_net_evaluate(NULL, inputs, g_outputs), "in_net != NULL");
    ASSERT_PROBE_EXPECT(neural_net_evaluate(&g_net, NULL, g_outputs), "in_inputs != NULL");
    ASSERT_PROBE_EXPECT(neural_net_evaluate(&g_net, inputs, NULL), "out_outputs != NULL");
}
