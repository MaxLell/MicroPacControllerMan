/*
 * neural_net.h
 *
 * Evaluates a small feed-forward network held as `const` data
 * ([M6 §8](../../../Docu/Design/M6-Pacman-AI.md), FR-038/039, NFR-007/008).
 *
 * The generic primitive; `App/pacman_ai` is its instance, the way `Bsp/switch` is to
 * `user_button`. Nothing here knows about Pacman.
 *
 * **Arbitrary topology, not layers of matrices.** The networks this evaluates come from NEAT,
 * which grows connections wherever they earn their place, so there is no layer structure to
 * multiply. Instead every node carries the list of connections feeding it, and nodes are
 * evaluated in index order — which works because the exporter is required to number them so
 * that a connection's source always has a lower index than its target. That requirement is
 * checked (#neural_net_is_well_formed) rather than trusted, because a network that violates it
 * would read an uninitialised value and still produce a plausible-looking answer.
 *
 * **Determinism is the point.** FR-039 asks the target to choose the same action as the host, so
 * the evaluation order is fixed, every value is `float` rather than `double`, and the only
 * activation is ReLU — which is exact in float32 on both machines, unlike `tanh` or `sigmoid`,
 * where the host's libm and the target's newlib are not required to agree to the last bit.
 */

#ifndef NEURAL_NET_H
#define NEURAL_NET_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * neural_net - public types
 * ========================================================================= */

/*! \brief Most nodes a network may have, inputs included.
 *
 * Bounds the one scratch buffer, which is reserved statically because the target does not
 * allocate (NFR-008). NEAT grows networks slowly and the exporter fails loudly rather than
 * silently truncating, so this is a ceiling and not a target.
 */
#define NEURAL_NET_MAX_NODES (128U)

/*! \brief A network: node values, biases, and the connections feeding each node.
 *
 * The connection lists are stored the way a sparse matrix is: `connection_offsets` has
 * `node_count + 1` entries, and the connections feeding node `i` are those with indices in
 * `[connection_offsets[i], connection_offsets[i + 1])`. Inputs have none.
 *
 * Every pointer is expected to be `const` data in flash. The struct itself carries no state —
 * evaluating twice with the same inputs gives the same answer, which is what VT-UNIT-011 checks.
 */
typedef struct
{
    uint16_t input_count;  /*!< Nodes 0..input_count-1 are the inputs        */
    uint16_t node_count;   /*!< Total, inputs included                        */
    uint16_t output_count; /*!< How many entries `output_nodes` has           */

    /*! \brief Which nodes the outputs are read from, in the caller's order. Not necessarily the
     *         last nodes: NEAT may add hidden nodes after the outputs were created. */
    const uint16_t* output_nodes;

    /*! \brief One per node. The inputs' entries are never read. */
    const float* biases;

    const uint16_t* connection_offsets; /*!< `node_count + 1` entries        */
    const uint16_t* connection_sources; /*!< One per connection             */
    const float* connection_weights;    /*!< One per connection             */
} neural_net_t;

/* ==========================================================================
 * neural_net - public API
 * ========================================================================= */

/*! \brief Whether a network can be evaluated at all.
 *
 * Checks the things a broken exporter would get wrong: sizes within bounds, offsets ascending,
 * every output node in range, and — the one that matters — every connection's source numbered
 * below the node it feeds. Cheap enough to call once at start-up, which is what
 * `App/pacman_ai` does, so that a bad table fails where it can be reported rather than by
 * playing badly.
 *
 * \param[in]       in_net: the network, must not be `NULL`
 * \return          `true` when the network is safe to evaluate
 */
bool neural_net_is_well_formed(const neural_net_t* in_net);

/*! \brief Run the network.
 *
 * \param[in]       in_net: a network #neural_net_is_well_formed accepts, must not be `NULL`
 * \param[in]       in_inputs: `input_count` values, must not be `NULL`
 * \param[out]      out_outputs: `output_count` values, must not be `NULL`
 */
void neural_net_evaluate(const neural_net_t* in_net, const float* in_inputs, float* out_outputs);

#endif /* NEURAL_NET_H */
