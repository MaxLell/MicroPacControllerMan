"""A NEAT genome as ``Services/neural_net`` wants it.

One conversion, two consumers: the trainer pushes the arrays into ``libpacman_env.so`` so that
every genome is scored by the *same* evaluator the firmware runs, and ``export_c.py`` writes the
winner out as ``const`` C from the same function. That is deliberate — if training used
neat-python's own network and only the export went through here, the two could disagree and the
disagreement would first show up on the board (FR-039).

The one thing this module has to get right is the node numbering: ``neural_net`` evaluates nodes in
index order and therefore requires every connection's source to be numbered below its target. The
ordering below is inputs first, then neat-python's own feed-forward layers, which is exactly that
property — and the C side re-checks it rather than trusting us.

See Docu/Design/M6-Pacman-AI.md §8.
"""

import hashlib
import struct
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

from neat.graphs import feed_forward_layers

#: What the C evaluator can hold — NEURAL_NET_MAX_NODES.
MAX_NODES = 128

#: The only activation the port is allowed to use, because it is the only one that is exact in
#: float32 on both machines. See M6 §9.
REQUIRED_ACTIVATION = "relu"


@dataclass
class Net:
    """A feed-forward network flattened the way ``neural_net_t`` holds one.

    ``connection_offsets`` is ``node_count + 1`` long: the connections feeding node ``i`` are the
    entries ``[connection_offsets[i], connection_offsets[i + 1])`` of the two connection arrays.
    Inputs have none.
    """

    input_count: int
    output_count: int
    biases: List[float]
    output_nodes: List[int]
    connection_offsets: List[int]
    connection_sources: List[int]
    connection_weights: List[float]

    #: Which genome node each index came from. Not used by the evaluator — it is what makes an
    #: exported table readable next to the genome it came from.
    node_keys: List[int] = field(default_factory=list)

    @property
    def node_count(self) -> int:
        return len(self.biases)

    @property
    def connection_count(self) -> int:
        return len(self.connection_sources)

    @property
    def hidden_count(self) -> int:
        return self.node_count - self.input_count - self.output_count

    def digest(self) -> str:
        """A short hash of everything that decides an action.

        Written into the generated C so that a table on the board can be tied back to the run that
        produced it — the same reason `ott pacman` prints its maze seed.
        """
        digest = hashlib.sha256()
        digest.update(struct.pack("<III", self.input_count, self.node_count, self.output_count))
        digest.update(struct.pack(f"<{len(self.output_nodes)}H", *self.output_nodes))
        digest.update(struct.pack(f"<{len(self.biases)}f", *self.biases))
        digest.update(struct.pack(f"<{len(self.connection_offsets)}H", *self.connection_offsets))
        if self.connection_count:
            digest.update(struct.pack(f"<{self.connection_count}H", *self.connection_sources))
            digest.update(struct.pack(f"<{self.connection_count}f", *self.connection_weights))

        return digest.hexdigest()[:16]


def from_dict(payload: dict) -> Net:
    """Read back what ``train.py`` wrote — the way `evaluate.py` and `export_c.py` get a network.

    They go through the same :class:`Net` as training did, so a winner is measured and exported as
    the object it was trained as, not as a second reading of a file.
    """
    return Net(
        input_count=int(payload["input_count"]),
        output_count=int(payload["output_count"]),
        biases=[float(value) for value in payload["biases"]],
        output_nodes=[int(value) for value in payload["output_nodes"]],
        connection_offsets=[int(value) for value in payload["connection_offsets"]],
        connection_sources=[int(value) for value in payload["connection_sources"]],
        connection_weights=[float(value) for value in payload["connection_weights"]],
        node_keys=[int(value) for value in payload.get("node_keys", [])],
    )


def _node_order(config, enabled: List[Tuple[int, int]]) -> List[int]:
    """Inputs, then the layers that can be evaluated, then any output nothing reaches.

    An unreachable output is kept rather than dropped: the action set is fixed, so the evaluator
    must produce four numbers whatever the topology happens to be. Its value is its own bias
    through the activation, which is what the C side computes for a node with no connections.
    """
    genome_config = config.genome_config
    order = list(genome_config.input_keys)

    # neat-python 2.0 returns `(layers, required)` here although its docstring says it returns the
    # layers — unpacked rather than iterated, because iterating gives the layer list and then the
    # required-node set, and the set looks enough like a layer to get quite far before failing.
    layers, _required = feed_forward_layers(genome_config.input_keys, genome_config.output_keys, enabled)

    for layer in layers:
        # Sorted so that two runs of the same genome produce byte-identical tables; a set's
        # iteration order is not something to hang a hash on.
        order.extend(sorted(layer))

    order.extend(key for key in genome_config.output_keys if key not in order)

    return order


def from_genome(genome, config) -> Net:
    """Flatten a genome. Raises ``ValueError`` on anything the port could not reproduce."""
    genome_config = config.genome_config
    enabled = [key for key, gene in genome.connections.items() if gene.enabled]
    order = _node_order(config, enabled)

    if len(order) > MAX_NODES:
        raise ValueError(f"{len(order)} nodes, but the evaluator holds {MAX_NODES}")

    index_of: Dict[int, int] = {key: index for index, key in enumerate(order)}
    input_count = len(genome_config.input_keys)

    incoming: Dict[int, List[Tuple[int, float]]] = {key: [] for key in order}
    for (source, target) in enabled:
        if (source in index_of) and (target in index_of) and (index_of[target] >= input_count):
            incoming[target].append((index_of[source], genome.connections[(source, target)].weight))

    biases: List[float] = []
    offsets: List[int] = [0]
    sources: List[int] = []
    weights: List[float] = []

    for key in order:
        node = genome.nodes.get(key)

        if node is None:
            # An input: it carries no bias and nothing feeds it.
            biases.append(0.0)
            offsets.append(len(sources))
            continue

        # These three are pinned in the configuration because the C evaluator implements none of
        # them. Checked rather than assumed, since a config edit is exactly how they would come
        # back, and the resulting network would then play differently on the board than it trained.
        if node.activation != REQUIRED_ACTIVATION:
            raise ValueError(f"node {key} activates with {node.activation!r}, not {REQUIRED_ACTIVATION!r}")
        if node.aggregation != "sum":
            raise ValueError(f"node {key} aggregates with {node.aggregation!r}, not 'sum'")
        if abs(node.response - 1.0) > 1e-9:
            raise ValueError(f"node {key} has response {node.response}, and the evaluator has no response term")

        biases.append(float(node.bias))

        # Sorted by source index: the evaluator sums in array order and float addition is not
        # associative, so the order is part of what makes the answer reproducible.
        for source_index, weight in sorted(incoming[key]):
            sources.append(source_index)
            weights.append(float(weight))

        offsets.append(len(sources))

    return Net(
        input_count=input_count,
        output_count=len(genome_config.output_keys),
        biases=biases,
        output_nodes=[index_of[key] for key in genome_config.output_keys],
        connection_offsets=offsets,
        connection_sources=sources,
        connection_weights=weights,
        node_keys=order,
    )


# --- a fixed topology, for a search that is not NEAT ---------------------------------------------

def dense_value_count(input_count: int, hidden_count: int, output_count: int) -> int:
    """How many numbers :func:`dense` needs.

    Every weight and every bias of a fully connected `input -> hidden -> output` network. For the
    firmware's 23 features and 4 actions with 16 hidden units that is 452 — which is the point of
    using it: a NEAT winner measured on this game was down to **6** connections out of 23 inputs,
    because NEAT deletes structure whenever fitness is noisy and a deletion that costs real ability
    is invisible in noise. A fixed topology cannot prune itself blind.
    """
    return (input_count * hidden_count) + hidden_count + (hidden_count * output_count) + output_count


def dense(input_count: int, hidden_count: int, output_count: int, values: List[float]) -> Net:
    """A fully connected `input -> hidden -> output` network from a flat vector.

    The node numbering is inputs, then hidden, then outputs, which satisfies the evaluator's rule
    that a connection's source is numbered below its target — the same rule `from_genome` obeys, and
    the C side re-checks it either way.

    `values` is laid out as: the hidden layer's weights row by row, the hidden biases, the output
    layer's weights row by row, the output biases. Any optimiser working on a flat vector can produce
    one; nothing here knows which.
    """
    expected = dense_value_count(input_count, hidden_count, output_count)

    if len(values) != expected:
        raise ValueError(f"expected {expected} values for {input_count}-{hidden_count}-{output_count}, "
                         f"got {len(values)}")

    node_count = input_count + hidden_count + output_count

    if node_count > MAX_NODES:
        raise ValueError(f"{node_count} nodes exceeds the evaluator's {MAX_NODES}")

    biases = [0.0] * node_count
    offsets = [0]
    sources: List[int] = []
    weights: List[float] = []
    cursor = 0

    # Inputs feed nothing and have no bias; their offsets are all zero, which is what the evaluator
    # reads as "no incoming connections".
    for _ in range(input_count):
        offsets.append(0)

    for hidden in range(hidden_count):
        for source in range(input_count):
            sources.append(source)
            weights.append(values[cursor])
            cursor += 1

        offsets.append(len(sources))

    for hidden in range(hidden_count):
        biases[input_count + hidden] = values[cursor]
        cursor += 1

    for output in range(output_count):
        for hidden in range(hidden_count):
            sources.append(input_count + hidden)
            weights.append(values[cursor])
            cursor += 1

        offsets.append(len(sources))

    for output in range(output_count):
        biases[input_count + hidden_count + output] = values[cursor]
        cursor += 1

    return Net(
        input_count=input_count,
        output_count=output_count,
        biases=biases,
        output_nodes=[input_count + hidden_count + index for index in range(output_count)],
        connection_offsets=offsets,
        connection_sources=sources,
        connection_weights=weights,
        # Not a genome, so there are no genome keys to record. The node index is the only name a node
        # here has, which an exported table shows anyway.
        node_keys=list(range(node_count)),
    )
