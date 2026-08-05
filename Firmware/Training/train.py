"""Evolve a Pacman agent against the firmware's own game.

    python3 train.py                       # the whole curriculum, all cores
    python3 train.py --stage 3 --generations 200
    python3 train.py --workers 4 --out winner.json

Nothing here decides how the game behaves or how a network is evaluated — both of those are C, and
the same C the board runs (FR-039). This file is the loop around them: which rules a generation
plays under, which mazes it plays, and which genome is kept.

Two choices are worth knowing about before reading the code.

**The mazes change every generation.** Every level of the shipped game is a generated maze (FR-029),
so an agent that has learned one maze has learned nothing. Each generation draws a fresh set of
seeds, and every genome in that generation plays the same set — so genomes are comparable with each
other while nothing is comparable across generations. That is the right way round: selection only
ever needs the former.

**Training never sees the acceptance seeds.** 1000..1019 belong to VT-UNIT-010 and are excluded
here, because an agent measured on the mazes it was trained on tells you nothing about FR-029.

See Docu/Design/M6-Pacman-AI.md §2, §6 and §7.
"""

import argparse
import json
import multiprocessing
import os
import random
import sys
import time
from typing import List, Sequence

import neat

import net
from pacman_env import PacmanEnv, STAGE_FULL, STAGE_GHOSTS, STAGE_MAZE_ONLY

_HERE = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_CONFIG = os.path.join(_HERE, "config-neat.txt")

#: Reserved for VT-UNIT-010. Training draws its mazes from anywhere else.
ACCEPTANCE_SEEDS = range(1000, 1020)

#: Mazes one genome is scored on per generation. A compromise: one maze makes fitness a lottery
#: decided by which maze it was, and every extra maze costs a full episode per genome.
MAZES_PER_GENOME = 4

#: The curriculum of M6 §6. A stage ends when the best genome's fitness reaches `promote_at` or
#: when `generations` are spent, whichever comes first — the cap is there so that a stage which
#: turns out to be unlearnable cannot swallow the whole run in silence.
CURRICULUM = [
    {"stage": STAGE_MAZE_ONLY, "generations": 60, "promote_at": 1800.0, "what": "walk and eat"},
    {"stage": STAGE_GHOSTS, "generations": 120, "promote_at": 1800.0, "what": "stay alive"},
    {"stage": STAGE_FULL, "generations": 320, "promote_at": None, "what": "the whole game"},
]

# One environment per worker process, created on first use. The C side keeps its search scratch and
# the evaluator's node values at file scope, so a batch is not safe to share between threads — with
# processes the question does not arise, which is why this is a Pool and not a ThreadPool.
_WORKER_ENV = None


def _worker_env(count: int, library_path: str) -> PacmanEnv:
    global _WORKER_ENV

    if (_WORKER_ENV is None) or (_WORKER_ENV.count != count):
        if _WORKER_ENV is not None:
            _WORKER_ENV.close()

        _WORKER_ENV = PacmanEnv(count, library_path)

    return _WORKER_ENV


def _play(task):
    """One genome's whole evaluation, in a worker process.

    The network arrives already flattened rather than as a genome: `net.from_genome` then runs in
    the parent, where a topology it refuses is one visible exception instead of a worker dying.
    """
    flat_net, seeds, stage, library_path = task
    env = _worker_env(len(seeds), library_path)
    env.set_net(flat_net)
    scores, steps, levels = env.run(seeds, stage)

    return scores, steps, levels


class Trainer:
    """Holds what a generation needs and reports what it did."""

    def __init__(self, config, arguments):
        self.config = config
        self.arguments = arguments
        self.stage = arguments.stage or CURRICULUM[0]["stage"]
        self.random = random.Random(arguments.seed)
        self.generation = 0
        self.best_fitness = float("-inf")
        self.best_net = None
        self.best_report = {}
        self.pool = multiprocessing.Pool(arguments.workers) if arguments.workers > 1 else None

    def draw_seeds(self) -> List[int]:
        """Fresh mazes for this generation, none of them an acceptance maze."""
        seeds = []

        while len(seeds) < MAZES_PER_GENOME:
            candidate = self.random.randrange(1, 1_000_000)

            if (candidate not in ACCEPTANCE_SEEDS) and (candidate not in seeds):
                seeds.append(candidate)

        return seeds

    def evaluate(self, genomes, config) -> None:
        seeds = self.draw_seeds()
        started = time.perf_counter()

        flat = []
        for genome_id, genome in genomes:
            flat.append((net.from_genome(genome, config), seeds, self.stage, self.arguments.library))

        if self.pool is not None:
            results = self.pool.map(_play, flat, chunksize=1)
        else:
            results = [_play(task) for task in flat]

        total_steps = 0
        best_index = 0

        for index, ((genome_id, genome), (scores, steps, levels)) in enumerate(zip(genomes, results)):
            # Fitness is the mean score over this generation's mazes — FR-036's "maximise the
            # score", widened to "on a maze you have not seen", which is what FR-029 demands.
            genome.fitness = sum(scores) / float(len(scores))
            total_steps += sum(steps)

            if genome.fitness > genomes[best_index][1].fitness:
                best_index = index

        best_genome = genomes[best_index][1]
        best_scores, best_steps, best_levels = results[best_index]
        elapsed = time.perf_counter() - started

        if best_genome.fitness > self.best_fitness:
            self.best_fitness = best_genome.fitness
            self.best_net = flat[best_index][0]
            self.best_report = {
                "stage": self.stage,
                "generation": self.generation,
                "fitness": best_genome.fitness,
                "scores": best_scores,
                "levels": best_levels,
                "seeds": list(seeds),
            }

        self.generation += 1

        print(
            f"  stage {self.stage} gen {self.generation:4d}  "
            f"best {best_genome.fitness:8.1f}  scores {best_scores}  level {max(best_levels)}  "
            f"nodes {len(best_genome.nodes)} conns {sum(1 for c in best_genome.connections.values() if c.enabled)}  "
            f"{total_steps / elapsed:7.0f} decisions/s  {elapsed:5.1f}s",
            flush=True,
        )

    def close(self) -> None:
        if self.pool is not None:
            self.pool.close()
            self.pool.join()


def _write_winner(path: str, flat_net, report: dict, arguments) -> None:
    """The winner as plain JSON, so that `export_c.py` needs neither neat-python nor a pickle."""
    payload = {
        "digest": flat_net.digest(),
        "input_count": flat_net.input_count,
        "output_count": flat_net.output_count,
        "node_count": flat_net.node_count,
        "hidden_count": flat_net.hidden_count,
        "biases": flat_net.biases,
        "output_nodes": flat_net.output_nodes,
        "connection_offsets": flat_net.connection_offsets,
        "connection_sources": flat_net.connection_sources,
        "connection_weights": flat_net.connection_weights,
        "node_keys": flat_net.node_keys,
        "training": {**report, "population": arguments.population_size, "mazes_per_genome": MAZES_PER_GENOME},
    }

    with open(path, "w") as handle:
        json.dump(payload, handle, indent=2)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stage", type=int, choices=[1, 2, 3], help="train one stage only")
    parser.add_argument("--generations", type=int, help="override the stage's generation cap")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--seed", type=int, default=1, help="the draw of training mazes (FR-114)")
    parser.add_argument("--config", default=_DEFAULT_CONFIG)
    parser.add_argument("--out", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument("--checkpoint-every", type=int, default=25, help="generations, 0 to switch it off")
    parser.add_argument(
        "--library",
        default=os.path.join(os.path.dirname(_HERE), "build-host", "libpacman_env.so"),
        help="the game as a shared library",
    )
    arguments = parser.parse_args(argv)

    config = neat.Config(
        neat.DefaultGenome, neat.DefaultReproduction, neat.DefaultSpeciesSet, neat.DefaultStagnation, arguments.config
    )
    arguments.population_size = config.pop_size

    # Asked, not assumed. If the firmware's observation grows a feature, this is where the run
    # stops — rather than in a network that was trained against the wrong 23 numbers.
    probe = PacmanEnv(1, arguments.library)
    if config.genome_config.num_inputs != probe.feature_count:
        print(
            f"config has {config.genome_config.num_inputs} inputs, the firmware has "
            f"{probe.feature_count} features",
            file=sys.stderr,
        )
        return 1
    if config.genome_config.num_outputs != probe.action_count:
        print(
            f"config has {config.genome_config.num_outputs} outputs, the firmware has "
            f"{probe.action_count} actions",
            file=sys.stderr,
        )
        return 1
    probe.close()

    population = neat.Population(config)
    population.add_reporter(neat.StatisticsReporter())

    if arguments.checkpoint_every > 0:
        population.add_reporter(
            neat.Checkpointer(
                arguments.checkpoint_every, filename_prefix=os.path.join(_HERE, "checkpoint-neat-")
            )
        )

    trainer = Trainer(config, arguments)
    stages = [entry for entry in CURRICULUM if (arguments.stage is None) or (entry["stage"] == arguments.stage)]
    started = time.perf_counter()

    try:
        for entry in stages:
            trainer.stage = entry["stage"]
            generations = arguments.generations or entry["generations"]
            promote_at = entry["promote_at"]

            print(f"\n=== stage {entry['stage']}: {entry['what']} "
                  f"({generations} generations max, {arguments.workers} workers) ===", flush=True)

            # Run a generation at a time so that the stage can end the moment it is learned. `run`
            # would otherwise only come back when the whole cap is spent.
            for _ in range(generations):
                population.run(trainer.evaluate, 1)

                if (promote_at is not None) and (trainer.best_fitness >= promote_at):
                    print(f"  promoted: {trainer.best_fitness:.1f} >= {promote_at:.1f}", flush=True)
                    break

            # A stage is its own comparison. Carrying the previous stage's best fitness into the
            # next one would make the promotion test meaningless, since the rules just changed.
            trainer.best_fitness = float("-inf")
            if trainer.best_net is not None:
                _write_winner(arguments.out, trainer.best_net, trainer.best_report, arguments)
    except KeyboardInterrupt:
        print("\ninterrupted — writing what has been reached so far", flush=True)
    finally:
        trainer.close()

    if trainer.best_net is None:
        print("no genome was evaluated", file=sys.stderr)
        return 1

    _write_winner(arguments.out, trainer.best_net, trainer.best_report, arguments)

    print(
        f"\n{arguments.out}: {trainer.best_net.node_count} nodes "
        f"({trainer.best_net.hidden_count} hidden), {trainer.best_net.connection_count} connections, "
        f"digest {trainer.best_net.digest()}\n"
        f"{trainer.best_report}\n"
        f"{(time.perf_counter() - started) / 60.0:.1f} minutes",
        flush=True,
    )

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
