/*
 * neural_net.c — see neural_net.h.
 */

#include "neural_net.h"

#include <stddef.h>

#include "custom_assert.h"

/* Node values for one evaluation. File-scope rather than on the stack: the target reserves its
 * buffers statically (NFR-008), and 128 floats is more than belongs in a frame.
 *
 * This makes evaluation non-reentrant, which is fine on both sides — the firmware evaluates once
 * per decision from its one loop, and training runs concurrent games in separate processes. */
static float g_values[NEURAL_NET_MAX_NODES];

bool neural_net_is_well_formed(const neural_net_t* in_net)
{
    ASSERT(in_net != NULL);

    if ((in_net->node_count == 0U) || (in_net->node_count > NEURAL_NET_MAX_NODES))
    {
        return false;
    }

    if ((in_net->input_count > in_net->node_count) || (in_net->output_count == 0U))
    {
        return false;
    }

    if ((in_net->output_nodes == NULL) || (in_net->biases == NULL) || (in_net->connection_offsets == NULL))
    {
        return false;
    }

    for (uint16_t node = 0U; node < in_net->node_count; ++node)
    {
        const uint16_t begin = in_net->connection_offsets[node];
        const uint16_t end = in_net->connection_offsets[node + 1U];

        if (end < begin)
        {
            return false;
        }

        /* An input is a value handed in, not something computed, so a connection feeding one
         * would be silently ignored — better to reject the table than to evaluate a network
         * that is not the one that was trained. */
        if ((node < in_net->input_count) && (end != begin))
        {
            return false;
        }

        if ((end > begin) && ((in_net->connection_sources == NULL) || (in_net->connection_weights == NULL)))
        {
            return false;
        }

        for (uint16_t index = begin; index < end; ++index)
        {
            /* The invariant the whole evaluation rests on: a source is already computed by the
             * time it is read. Without this a network could read an uninitialised value and
             * still look like it worked. */
            if (in_net->connection_sources[index] >= node)
            {
                return false;
            }
        }
    }

    for (uint16_t index = 0U; index < in_net->output_count; ++index)
    {
        if (in_net->output_nodes[index] >= in_net->node_count)
        {
            return false;
        }
    }

    return true;
}

void neural_net_evaluate(const neural_net_t* in_net, const float* in_inputs, float* out_outputs)
{
    ASSERT(in_net != NULL);
    ASSERT(in_inputs != NULL);
    ASSERT(out_outputs != NULL);
    ASSERT(in_net->node_count <= NEURAL_NET_MAX_NODES);

    for (uint16_t node = 0U; node < in_net->input_count; ++node)
    {
        g_values[node] = in_inputs[node];
    }

    for (uint16_t node = in_net->input_count; node < in_net->node_count; ++node)
    {
        /* Written as an explicit float accumulation in a fixed order. Both halves matter for
         * FR-039: `double` would make the host disagree with the target's single-precision FPU,
         * and reordering the sum would change the last bits. */
        float sum = in_net->biases[node];

        const uint16_t begin = in_net->connection_offsets[node];
        const uint16_t end = in_net->connection_offsets[node + 1U];

        for (uint16_t index = begin; index < end; ++index)
        {
            sum += in_net->connection_weights[index] * g_values[in_net->connection_sources[index]];
        }

        /* ReLU, and only ReLU — exact in float32 and therefore identical on both machines,
         * which `tanh` and `sigmoid` out of two different libm implementations are not. */
        g_values[node] = (sum > 0.0F) ? sum : 0.0F;
    }

    for (uint16_t index = 0U; index < in_net->output_count; ++index)
    {
        out_outputs[index] = g_values[in_net->output_nodes[index]];
    }
}
