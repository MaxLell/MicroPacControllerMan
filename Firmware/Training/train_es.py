"""Evolve a **fixed** network with an evolution strategy, instead of growing one with NEAT.

    python3 train_es.py                          # the whole curriculum, all cores
    python3 train_es.py --hidden 24 --population 48
    python3 train_es.py --episode whole-run --max-seconds 27000

**Why this exists.** A NEAT winner measured on this game came out with **6 connections** out of 23
inputs — a network that is very nearly blind, and which still scores about 1,700 because wandering
around a full maze pays. The log shows how: 92 connections at the start, then 14, 18, 22, 19. NEAT
deletes structure, and under a noisy fitness a deletion that costs real ability is invisible, so it
keeps deleting. [DEC-044](../../Docu/PrePlanning/11-Decisions-and-As-Built.md) diagnosed exactly this
once already; FR-044's timing jitter put the noise back.

A fixed topology cannot prune itself blind. Every one of its 452 numbers is used on every decision,
and the search moves all of them at once.

**What the search is.** A separable evolution strategy: a mean vector and one standard deviation per
dimension, sampled, ranked, and recombined with the log-weights CMA-ES uses. It is **not** CMA-ES —
there is no covariance matrix, which is what CMA-ES's name is about — and that is deliberate: the full
method wants numpy, numpy is not in the container, and adding it means a rebuilt image and a
dependency for an eigendecomposition of a 452 x 452 matrix that this problem does not need. On a
separable problem with a cheap noisy objective, the diagonal variant is what people reach for anyway.

Everything else is shared with `train.py` rather than copied: the episode, the fitness, the ghost
bonus, the curriculum and the winner file. Two trainers that scored genomes differently would make
their results incomparable, which is the one thing this experiment must not do.

See Docu/Design/M6-Pacman-AI.md §2 and §14.
"""

import argparse
import math
import multiprocessing
import os
import random
import sys
import time
from typing import List, Sequence

import net
from pacman_env import STAGE_FULL

# Imported rather than restated. `_play` and `_write_winner` are private to `train.py` by name only:
# sharing them is the whole point, because a second copy of "what a genome is worth" or "what a winner
# file looks like" is a second thing to keep in step, and the comparison between the two trainers
# depends on there being exactly one of each.
from train import (ACCEPTANCE_SEEDS, CURRICULUM, EPISODES_PER_GENOME, GHOST_BONUS, _play,
                   _write_winner, stage_deadlines)

_HERE = os.path.dirname(os.path.abspath(__file__))

#: Hidden units. Sixteen makes 452 numbers against the 23 features and 4 actions — three orders of
#: magnitude inside NFR-007's footprint, and small enough that the exported table stays a few hundred
#: bytes like the NEAT ones.
HIDDEN_UNITS = 16

#: Candidates per generation, and how many of them are recombined. `population` is deliberately far
#: below NEAT's 250: a generation here costs `population * episodes` episodes, so 32 buys about eight
#: times as many generations in the same night.
POPULATION = 32
ELITE = 8


class Strategy:
    """The search: a mean, a standard deviation per dimension, and ranked recombination.

    Initialised the way a ReLU network wants to be. The weights start at zero mean with He's
    deviation — `sqrt(2 / fan_in)` — because that is what keeps a ReLU layer's output from collapsing
    or exploding as it is sampled. The **output biases start at 1.0**, and that is not cosmetic: the
    evaluator applies ReLU to the output nodes too, so a network whose outputs are all negative has
    four zeros and the action is decided by the tie-break rather than by the network. Starting them
    positive means every sample says something.
    """

    def __init__(self, input_count: int, hidden_count: int, output_count: int, rng: random.Random):
        self.rng = rng
        self.size = net.dense_value_count(input_count, hidden_count, output_count)

        hidden_weights = input_count * hidden_count
        hidden_biases = hidden_count
        output_weights = hidden_count * output_count

        self.mean = [0.0] * self.size
        self.sigma = [0.0] * self.size

        for index in range(self.size):
            if index < hidden_weights:
                self.sigma[index] = math.sqrt(2.0 / input_count)
            elif index < hidden_weights + hidden_biases:
                self.sigma[index] = 0.1
            elif index < hidden_weights + hidden_biases + output_weights:
                self.sigma[index] = math.sqrt(2.0 / hidden_count)
            else:
                self.sigma[index] = 0.1
                self.mean[index] = 1.0

        # Log-linear, as CMA-ES weights its parents: the best candidate counts for several times what
        # the last of the elite does. A flat mean over the elite is the cross-entropy method and is
        # measurably worse at the same cost.
        raw = [math.log(ELITE + 0.5) - math.log(rank + 1) for rank in range(ELITE)]
        total = sum(raw)
        self.weights = [value / total for value in raw]

        #: How much of a generation's measured spread replaces the current one. All of it makes the
        #: search collapse the first time a generation happens to agree; none of it never adapts.
        self.adaptation = 0.5

        #: A floor, so a dimension that stopped mattering can start mattering again. Without it the
        #: search is one unlucky generation away from being unable to move in that direction ever.
        self.sigma_floor = 0.01

    def sample(self, count: int) -> List[List[float]]:
        return [[self.mean[index] + (self.sigma[index] * self.rng.gauss(0.0, 1.0))
                 for index in range(self.size)]
                for _ in range(count)]

    def tell(self, candidates: Sequence[Sequence[float]], fitnesses: Sequence[float]) -> None:
        ranked = sorted(range(len(candidates)), key=lambda index: fitnesses[index], reverse=True)
        elite = [candidates[index] for index in ranked[:ELITE]]

        previous = self.mean

        self.mean = [sum(self.weights[rank] * elite[rank][index] for rank in range(len(elite)))
                     for index in range(self.size)]

        # Measured around the *previous* mean rather than the new one: the step the search just took
        # is part of the spread it should keep exploring, and measuring around the new mean throws
        # that away and shrinks every time it makes progress.
        for index in range(self.size):
            spread = math.sqrt(sum(self.weights[rank] * ((elite[rank][index] - previous[index]) ** 2)
                                   for rank in range(len(elite))))

            moved = ((1.0 - self.adaptation) * self.sigma[index]) + (self.adaptation * spread)
            self.sigma[index] = max(moved, self.sigma_floor)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stage", type=int, choices=[1, 2, 3], help="train one stage only")
    parser.add_argument("--generations", type=int, help="override the stage's generation cap")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--hidden", type=int, default=HIDDEN_UNITS)
    parser.add_argument("--population", type=int, default=POPULATION)
    parser.add_argument("--episodes", type=int, default=EPISODES_PER_GENOME,
                        help="episodes each candidate is scored on per generation")
    parser.add_argument("--episode", choices=["one-life", "whole-run"], default="one-life",
                        help="stop an episode at the first death, or play the run out as FR-037 does")
    parser.add_argument("--max-seconds", type=float, default=None,
                        help="stop cleanly after this long, whatever generation it is on")
    parser.add_argument("--seed", type=int, default=1, help="the run's own draw")
    parser.add_argument("--out", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument(
        "--library",
        default=os.path.join(os.path.dirname(_HERE), "build-host", "libpacman_env.so"),
        help="the game as a shared library",
    )
    arguments = parser.parse_args(argv)

    # `_write_winner` reads this off the arguments, and the population here is the candidate count.
    arguments.population_size = arguments.population

    from pacman_env import PacmanEnv

    probe = PacmanEnv(1, arguments.library)
    input_count = probe.feature_count
    output_count = probe.action_count
    probe.close()

    rng = random.Random(arguments.seed)
    strategy = Strategy(input_count, arguments.hidden, output_count, rng)

    print(f"a fixed {input_count}-{arguments.hidden}-{output_count} network: {strategy.size} numbers, "
          f"all of them used on every decision", flush=True)

    pool = multiprocessing.Pool(arguments.workers) if arguments.workers > 1 else None
    stages = [entry for entry in CURRICULUM if (arguments.stage is None) or (entry["stage"] == arguments.stage)]
    started = time.perf_counter()
    best_net = None
    generation = 0

    def is_out_of_time() -> bool:
        return (arguments.max_seconds is not None) and ((time.perf_counter() - started) >= arguments.max_seconds)

    def draw_seeds() -> List[int]:
        seeds: List[int] = []

        while len(seeds) < arguments.episodes:
            candidate = rng.randrange(1, 1_000_000)

            if (candidate not in ACCEPTANCE_SEEDS) and (candidate not in seeds):
                seeds.append(candidate)

        return seeds

    deadlines = stage_deadlines(started, arguments.max_seconds, stages)

    try:
        for entry in stages:
            if is_out_of_time():
                break

            stage = entry["stage"]
            generations = arguments.generations or entry["generations"]
            promote_at = entry["promote_at"]
            deadline = deadlines[stage]

            print(f"\n=== stage {stage}: {entry['what']} ({generations} generations max, "
                  f"{arguments.workers} workers, {arguments.episode} episodes) ===", flush=True)

            # A stage is its own comparison, and the winner file follows it. Stage 1 has no ghosts
            # and no power pellets, so its networks clear level after level and score tens of
            # thousands; stage 3 scores a couple of thousand. A best kept *across* the stages is
            # therefore a stage-1 walker that nothing later can ever beat, and it is what the file
            # would keep — train.py resets for the same reason, and the two must agree.
            stage_best = float("-inf")

            for _ in range(generations):
                seeds = draw_seeds()
                candidates = strategy.sample(arguments.population)
                nets = [net.dense(input_count, arguments.hidden, output_count, values) for values in candidates]
                tasks = [(flat, seeds, stage, arguments.library, arguments.episode == "one-life")
                         for flat in nets]

                at = time.perf_counter()
                results = pool.map(_play, tasks, chunksize=1) if pool is not None else [_play(task) for task in tasks]
                elapsed = time.perf_counter() - at

                fitnesses = [result[0] for result in results]
                strategy.tell(candidates, fitnesses)

                best = max(range(len(fitnesses)), key=lambda index: fitnesses[index])
                generation += 1
                total_steps = sum(sum(result[2]) for result in results)

                _, scores, steps, levels, ghosts = results[best]
                mean_score = sum(scores) / float(len(scores))
                mean_sigma = sum(strategy.sigma) / float(strategy.size)

                if fitnesses[best] > stage_best:
                    stage_best = fitnesses[best]
                    best_net = nets[best]

                    # Written the moment it improves, so a killed run still leaves its best on disk.
                    _write_winner(arguments.out, best_net, {
                        "stage": stage,
                        "generation": generation,
                        "fitness": fitnesses[best],
                        "mean_score": mean_score,
                        "scores": scores,
                        "ghosts_eaten": sum(ghosts),
                        "level": max(levels),
                        "decisions": sum(steps),
                        "algorithm": "separable evolution strategy",
                        "hidden": arguments.hidden,
                    }, arguments)

                print(f"  stage {stage} gen {generation:4d}  best {fitnesses[best]:8.1f}  "
                      f"score {mean_score:7.1f}  ghosts {sum(ghosts):3d}  level {max(levels)}  "
                      f"sigma {mean_sigma:.3f}  {total_steps / elapsed:7.0f} decisions/s  {elapsed:5.1f}s",
                      flush=True)

                if (promote_at is not None) and (stage_best >= promote_at):
                    print(f"  promoted: {stage_best:.1f} >= {promote_at:.1f}", flush=True)
                    break

                if time.perf_counter() >= deadline:
                    print(f"  stage {stage} is out of time after "
                          f"{(time.perf_counter() - started) / 60.0:.0f} min of the run", flush=True)
                    break
    except KeyboardInterrupt:
        print("\ninterrupted — writing what has been reached so far", flush=True)
    finally:
        if pool is not None:
            pool.close()
            pool.join()

    if best_net is None:
        print("no candidate was evaluated", file=sys.stderr)
        return 1

    print(f"\n{arguments.out}: {best_net.node_count} nodes ({best_net.hidden_count} hidden), "
          f"{best_net.connection_count} connections, digest {best_net.digest()}\n"
          f"{(time.perf_counter() - started) / 60.0:.1f} minutes", flush=True)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
