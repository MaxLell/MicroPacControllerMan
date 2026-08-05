"""VT-UNIT-010: how strongly a trained agent actually plays.

    python3 evaluate.py                       # winner.json against the acceptance seeds
    python3 evaluate.py --winner other.json --stage 3

Measures two policies over the **same** twenty mazes in the **same** run — the trained network and
a uniform-random one — and holds the result against FR-037: at least 4,600 points on average, and
more than random. Both figures are taken here rather than one of them quoted from a document,
because a baseline measured somewhere else is a baseline that can drift (M6 §13).

Exit code 0 means the requirement is met, 1 means it is not, so this is usable as a gate.

The seeds are 1000..1019 and training is forbidden from drawing them (see train.py). That is the
whole point: every level of the shipped game is a maze the agent has never seen (FR-029), so a
score on a maze it *was* trained on would not answer the question FR-037 asks.
"""

import argparse
import json
import os
import statistics
import sys
from typing import List, Sequence

import net
from pacman_env import PacmanEnv, STAGE_FULL

_HERE = os.path.dirname(os.path.abspath(__file__))

#: The acceptance seed set of M6 §13. A contiguous range on purpose: changing it is one visible
#: line in a diff, whereas swapping an unkind seed out of a hand-picked list looks like nothing.
ACCEPTANCE_SEEDS = list(range(1000, 1020))

#: FR-037's bar.
REQUIRED_MEAN_SCORE = 4600.0


def _report(title: str, scores: Sequence[int], steps: Sequence[int], levels: Sequence[int], cap: int) -> float:
    mean = statistics.fmean(scores)

    print(f"{title}")
    print(f"  scores  {list(scores)}")
    print(f"  mean    {mean:.1f}   median {statistics.median(scores):.0f}   "
          f"min {min(scores)}   max {max(scores)}")
    print(f"  levels  reached up to {max(levels)}   decisions {min(steps)}..{max(steps)}")

    # A run stopped by the decision cap has not finished, so its score is a lower bound rather than
    # a result. Said out loud, because silently averaging truncated runs is how a harness reports a
    # number nobody can reproduce.
    truncated = [index for index, value in enumerate(steps) if value >= cap]
    if truncated:
        print(f"  NOTE    {len(truncated)} run(s) hit the {cap}-decision cap: seeds "
              f"{[ACCEPTANCE_SEEDS[index] for index in truncated]}")

    return mean


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--winner", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument("--stage", type=int, default=STAGE_FULL, choices=[1, 2, 3])
    parser.add_argument("--rng-seed", type=int, default=1, help="the random baseline's own draw")
    parser.add_argument(
        "--library",
        default=os.path.join(os.path.dirname(_HERE), "build-host", "libpacman_env.so"),
    )
    arguments = parser.parse_args(argv)

    if not os.path.exists(arguments.winner):
        print(f"{arguments.winner} is missing — run train.py first", file=sys.stderr)
        return 1

    with open(arguments.winner) as handle:
        payload = json.load(handle)

    trained = net.from_dict(payload)

    if payload.get("digest") not in (None, trained.digest()):
        print(f"{arguments.winner} says its digest is {payload['digest']} but the tables hash to "
              f"{trained.digest()}", file=sys.stderr)
        return 1

    seeds: List[int] = ACCEPTANCE_SEEDS

    with PacmanEnv(len(seeds), arguments.library) as env:
        cap = env.max_decisions
        print(f"{len(seeds)} runs on seeds {seeds[0]}..{seeds[-1]}, stage {arguments.stage}, "
              f"{trained.node_count} nodes ({trained.hidden_count} hidden), "
              f"{trained.connection_count} connections, digest {trained.digest()}\n")

        env.set_net(trained)
        trained_mean = _report("trained", *env.run(seeds, arguments.stage), cap=cap)

        print()

        env.use_random_policy(arguments.rng_seed)
        random_mean = _report("uniform random", *env.run(seeds, arguments.stage), cap=cap)

    beats_random = trained_mean > random_mean
    meets_bar = trained_mean >= REQUIRED_MEAN_SCORE

    print(f"\nFR-037: mean {trained_mean:.1f} vs. required {REQUIRED_MEAN_SCORE:.0f} — "
          f"{'met' if meets_bar else 'NOT met'}")
    print(f"         mean {trained_mean:.1f} vs. random {random_mean:.1f} "
          f"({trained_mean / random_mean:.1f}x) — {'met' if beats_random else 'NOT met'}")

    if arguments.stage != STAGE_FULL:
        # Said rather than silently passing: the earlier stages have fewer things worth points in
        # them, so their scores are not the figure FR-037 is about (M6 §6).
        print(f"\nstage {arguments.stage} is not stage 3, so this is not a VT-UNIT-010 result")
        return 0

    return 0 if (meets_bar and beats_random) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
