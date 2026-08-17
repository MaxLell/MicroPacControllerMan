"""VT-UNIT-010: how strongly a trained agent actually plays.

    python3 evaluate.py                       # winner.json on the normal maze
    python3 evaluate.py --winner other.json --stage 3
    python3 evaluate.py --maze generated      # the same agent on mazes nobody has played

Measures two policies on the **same** maze in the **same** run — the trained network and a uniform
random one — and holds the result against FR-037: at least 4,600 points, and more than random. Both
figures are taken here rather than one of them quoted from a document, because a baseline measured
somewhere else is a baseline that can drift (M6 §13).

Exit code 0 means the requirement is met, 1 means it is not, so this is usable as a gate.

**The maze is the normal one and the figure is a mean over twenty runs.** The AI may only be handed
control in the normal maze (FR-040), so that is the maze FR-037 is about. It was briefly measurable in
a single episode — one fixed maze and a game with nothing random in it play out identically every
time — and FR-044's timing jitter ended that: a run is a draw again, and both policies have a spread.

**A run here has its three lives.** Training may end an episode at the first death (FR-036), which is
how it makes dying cost something; that is a training rule. FR-037 asks what a *run* scores, so this
measures runs, whichever trainer produced the network.

``--maze generated`` answers a different and no longer required question — how the agent does on a
maze it has never seen — and says so rather than returning an FR-037 verdict.
"""

import argparse
import json
import os
import statistics
import sys
from typing import List, Sequence

import net
from pacman_env import MAZE_GENERATED, MAZE_NORMAL, PacmanEnv, STAGE_FULL

_HERE = os.path.dirname(os.path.abspath(__file__))

#: Runs each policy is averaged over. Both have a spread now — the trained one because the game's
#: timings vary from run to run, the baseline because it also picks its directions at random.
EPISODES = 20

#: Where ``--maze generated`` starts counting. A contiguous range on purpose: changing it is one
#: visible line in a diff, whereas swapping an unkind seed out of a hand-picked list looks like
#: nothing.
GENERATED_FIRST_SEED = 1000

#: FR-037's bar.
REQUIRED_MEAN_SCORE = 4600.0


def _report(title: str, scores: Sequence[int], steps: Sequence[int], levels: Sequence[int],
            ghosts: Sequence[int], cap: int) -> float:
    mean = statistics.fmean(scores)

    print(f"{title}")
    print(f"  scores  {list(scores)}")
    print(f"  mean    {mean:.1f}   median {statistics.median(scores):.0f}   "
          f"min {min(scores)}   max {max(scores)}")
    print(f"  levels  reached up to {max(levels)}   decisions {min(steps)}..{max(steps)}   "
          f"ghosts eaten {sum(ghosts)}")

    # A run stopped by the decision cap has not finished, so its score is a lower bound rather than
    # a result. Said out loud, because silently averaging truncated runs is how a harness reports a
    # number nobody can reproduce.
    truncated = sum(1 for value in steps if value >= cap)
    if truncated:
        print(f"  NOTE    {truncated} run(s) hit the {cap}-decision cap")

    return mean


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--winner", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument("--stage", type=int, default=STAGE_FULL, choices=[1, 2, 3])
    parser.add_argument("--maze", default="normal", choices=["normal", "generated"],
                        help="the maze FR-037 is about, or mazes the agent has never seen")
    parser.add_argument("--episodes", type=int, default=EPISODES,
                        help="runs each policy is averaged over")
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

    is_generated = arguments.maze == "generated"
    maze = MAZE_GENERATED if is_generated else MAZE_NORMAL

    # The same seeds for both policies, so the two are measured on the same runs of the same game and
    # a lucky draw cannot flatter one of them.
    seeds: List[int] = list(range(GENERATED_FIRST_SEED, GENERATED_FIRST_SEED + arguments.episodes))

    print(f"stage {arguments.stage}, {arguments.maze} maze, {trained.node_count} nodes "
          f"({trained.hidden_count} hidden), {trained.connection_count} connections, "
          f"digest {trained.digest()}\n")

    with PacmanEnv(arguments.episodes, arguments.library) as env:
        cap = env.max_decisions

        env.set_net(trained)
        trained_mean = _report("trained", *env.run(seeds, arguments.stage, maze), cap=cap)

        print()

        env.use_random_policy(arguments.rng_seed)
        random_mean = _report("uniform random", *env.run(seeds, arguments.stage, maze), cap=cap)

    beats_random = trained_mean > random_mean
    meets_bar = trained_mean >= REQUIRED_MEAN_SCORE

    print(f"\nFR-037: {trained_mean:.1f} vs. required {REQUIRED_MEAN_SCORE:.0f} — "
          f"{'met' if meets_bar else 'NOT met'}")
    print(f"         {trained_mean:.1f} vs. random {random_mean:.1f} "
          f"({trained_mean / max(random_mean, 1.0):.1f}x) — {'met' if beats_random else 'NOT met'}")

    if is_generated:
        # Said rather than silently passing or failing: FR-037 is about the maze the AI is allowed
        # to play, and this is not it.
        print("\nthe generated mazes are not what FR-037 asks about, so this is not a "
              "VT-UNIT-010 result")
        return 0

    if arguments.stage != STAGE_FULL:
        # Likewise: the earlier stages have fewer things worth points in them, so their scores are
        # not the figure FR-037 is about (M6 §6).
        print(f"\nstage {arguments.stage} is not stage 3, so this is not a VT-UNIT-010 result")
        return 0

    return 0 if (meets_bar and beats_random) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
