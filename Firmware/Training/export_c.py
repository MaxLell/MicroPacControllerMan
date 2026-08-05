"""Turn a trained winner into the `const` tables the firmware carries.

    python3 export_c.py                       # winner.json -> App/pacman_ai/ai_weights.[ch]
    python3 export_c.py --winner other.json --out-dir /tmp

Writes two files and nothing else: no allocation, no initialisation step, no parser on the target.
The network becomes data in flash, which is what makes NFR-007 a non-question and what lets
`neural_net_is_well_formed` check the table before the first frame rather than the first crash.

The weights are written as C hexadecimal float literals (`0x1.91eb86p-2f`). That is exact — a
decimal literal is a request to the compiler to find the nearest float, and "nearest" is one more
place where the host and the target could disagree about a network that FR-039 requires them to
agree on.
"""

import argparse
import json
import os
import struct
import sys
from typing import Sequence

import net

_HERE = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_OUT_DIR = os.path.join(os.path.dirname(_HERE), "App", "pacman_ai")

_BANNER = """/*
 * {name}
 *
 * GENERATED — do not edit. Written by Training/export_c.py from {source}.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network {digest}: {nodes} nodes ({hidden} hidden), {connections} connections,
 * {inputs} inputs, {outputs} outputs. Trained at stage {stage}, generation {generation},
 * fitness {fitness}.
 */
"""


def _float_literal(value: float) -> str:
    """A float32 written so that no compiler has to round it.

    Rounded to float32 first and then printed exactly: `float.hex()` prints the double, and the
    double is not what was trained with — the C evaluator, and therefore training itself, saw the
    float32.
    """
    single = struct.unpack("<f", struct.pack("<f", value))[0]

    return f"{single.hex()}f"


def _array(kind: str, name: str, values: Sequence, formatter, per_line: int) -> str:
    if not values:
        # An empty array is not C, and a null pointer is what `neural_net_t` expects when there is
        # nothing there.
        return ""

    body = ""
    for start in range(0, len(values), per_line):
        chunk = ", ".join(formatter(value) for value in values[start : start + per_line])
        body += f"    {chunk},\n"

    return f"static const {kind} {name}[{len(values)}] = {{\n{body}}};\n\n"


def _write_source(path: str, flat_net: net.Net, payload: dict, source_name: str) -> None:
    training = payload.get("training", {})
    text = _BANNER.format(
        name=os.path.basename(path),
        source=source_name,
        digest=flat_net.digest(),
        nodes=flat_net.node_count,
        hidden=flat_net.hidden_count,
        connections=flat_net.connection_count,
        inputs=flat_net.input_count,
        outputs=flat_net.output_count,
        stage=training.get("stage", "?"),
        generation=training.get("generation", "?"),
        fitness=f"{training.get('fitness', float('nan')):.1f}" if "fitness" in training else "?",
    )

    text += '\n#include "ai_weights.h"\n\n'
    text += _array("uint16_t", "g_output_nodes", flat_net.output_nodes, lambda value: f"{value}U", 8)
    text += _array("float", "g_biases", flat_net.biases, _float_literal, 4)
    text += _array("uint16_t", "g_connection_offsets", flat_net.connection_offsets, lambda value: f"{value}U", 8)
    text += _array("uint16_t", "g_connection_sources", flat_net.connection_sources, lambda value: f"{value}U", 8)
    text += _array("float", "g_connection_weights", flat_net.connection_weights, _float_literal, 4)

    has_connections = flat_net.connection_count > 0
    text += (
        "const neural_net_t g_ai_weights_network = {\n"
        f"    .input_count = {flat_net.input_count}U,\n"
        f"    .node_count = {flat_net.node_count}U,\n"
        f"    .output_count = {flat_net.output_count}U,\n"
        "    .output_nodes = g_output_nodes,\n"
        "    .biases = g_biases,\n"
        "    .connection_offsets = g_connection_offsets,\n"
        f"    .connection_sources = {'g_connection_sources' if has_connections else 'NULL'},\n"
        f"    .connection_weights = {'g_connection_weights' if has_connections else 'NULL'},\n"
        "};\n"
    )

    with open(path, "w") as handle:
        handle.write(text)


def _write_header(path: str, flat_net: net.Net, source_name: str) -> None:
    guard = "AI_WEIGHTS_H"
    text = _BANNER.format(
        name=os.path.basename(path),
        source=source_name,
        digest=flat_net.digest(),
        nodes=flat_net.node_count,
        hidden=flat_net.hidden_count,
        connections=flat_net.connection_count,
        inputs=flat_net.input_count,
        outputs=flat_net.output_count,
        stage="see the source file",
        generation="-",
        fitness="-",
    )

    text += (
        f"\n#ifndef {guard}\n#define {guard}\n\n"
        '#include <stddef.h>\n\n'
        '#include "neural_net.h"\n\n'
        "/*! \\brief The trained network — 23 features in, one score per relative action out. */\n"
        "extern const neural_net_t g_ai_weights_network;\n\n"
        "/*! \\brief Which table this is, so a board can be tied back to the run that trained it.\n"
        " *         The same figure `Training/evaluate.py` prints. */\n"
        f'#define AI_WEIGHTS_DIGEST "{flat_net.digest()}"\n\n'
        f"#endif /* {guard} */\n"
    )

    with open(path, "w") as handle:
        handle.write(text)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--winner", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument("--out-dir", default=_DEFAULT_OUT_DIR)
    arguments = parser.parse_args(argv)

    if not os.path.exists(arguments.winner):
        print(f"{arguments.winner} is missing — run train.py first", file=sys.stderr)
        return 1

    with open(arguments.winner) as handle:
        payload = json.load(handle)

    flat_net = net.from_dict(payload)

    if payload.get("digest") not in (None, flat_net.digest()):
        print(f"{arguments.winner} says its digest is {payload['digest']} but the tables hash to "
              f"{flat_net.digest()}", file=sys.stderr)
        return 1

    if flat_net.node_count > net.MAX_NODES:
        print(f"{flat_net.node_count} nodes, but the evaluator holds {net.MAX_NODES}", file=sys.stderr)
        return 1

    source_name = os.path.relpath(arguments.winner, os.path.dirname(_HERE))
    source_path = os.path.join(arguments.out_dir, "ai_weights.c")
    header_path = os.path.join(arguments.out_dir, "ai_weights.h")

    _write_source(source_path, flat_net, payload, source_name)
    _write_header(header_path, flat_net, source_name)

    # What the tables cost in flash, worked out here rather than guessed: it is the figure NFR-007
    # is about, and `arm-none-eabi-size` would only show it mixed in with everything else.
    flash_bytes = (
        len(flat_net.output_nodes) * 2
        + len(flat_net.biases) * 4
        + len(flat_net.connection_offsets) * 2
        + flat_net.connection_count * (2 + 4)
    )

    print(f"{source_path}\n{header_path}\n"
          f"{flat_net.node_count} nodes ({flat_net.hidden_count} hidden), "
          f"{flat_net.connection_count} connections, digest {flat_net.digest()}\n"
          f"{flash_bytes} bytes of tables")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
