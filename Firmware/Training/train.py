"""Evolve a Pac-Man agent against the firmware's own game.

    python3 train.py                       # the whole curriculum, all cores
    python3 train.py --stage 3 --generations 200
    python3 train.py --workers 4 --out winner.json

Nothing here decides how the game behaves or how a network is evaluated — both of those are C, and
the same C the board runs (FR-039). This file is the loop around them: which rules a generation
plays under, which mazes it plays, and which genome is kept.

Four things about it are worth knowing before reading the code.

**The maze is the normal one, always.** The AI is only ever handed control there (FR-040/FR-042), so
it is the only maze worth training on.

**Fitness is not the score.** It is the score *plus* a bonus for every ghost eaten (FR-036): the
game's own 200/400/800/1600 is the same currency as the pellets that produced it, so the score alone
cannot say "this was worth more". The plain score is still what `evaluate.py` measures, and it does not
know this bonus exists.

**An episode ends at the first death** by default, where a run has three lives — that is what makes
dying cost anything. `--episode whole-run` trains on what `evaluate.py` measures instead, and comparing the
two is an experiment rather than a setting.

**A genome is scored on twelve episodes, not one.** FR-044 jitters the game's timings, so a run is a
draw rather than a measurement. That noise is not a detail: it is what NEAT's structural search reads
when it decides whether a deletion cost anything, and a winner that pruned itself to six connections
is what happens when it cannot tell. `--no-deletion` forbids the pruning outright, which is the other
way at the same problem.

See Docu/Design/M6-Pacman-AI.md §2, §6, §7 and §14.
"""

import argparse
import json
import multiprocessing
import os
import random
import sys
import time
from typing import Sequence

import neat

import net
from pacman_env import MAZE_NORMAL, PacmanEnv, STAGE_FULL, STAGE_GHOSTS, STAGE_MAZE_ONLY

_HERE = os.path.dirname(os.path.abspath(__file__))
_DEFAULT_CONFIG = os.path.join(_HERE, "config-neat.txt")

#: What training pays for a ghost, on top of the 200/400/800/1600 the game's score already pays
#: (FR-036). Five hundred a ghost roughly doubles what a full chain of four is worth — 3,000 of score
#: becomes 5,000 of fitness — which is enough to change a decision without drowning out the pellets
#: that a level is mostly made of.
GHOST_BONUS = 500

#: Reserved for VT-UNIT-010, and reserved again. The acceptance set went away when the game became
#: deterministic and one episode was the whole measurement; the jitter brings it back, because an
#: episode is a draw again and a score on the draws it was trained against would answer nothing.
ACCEPTANCE_SEEDS = range(1000, 1020)

#: Episodes one genome is scored on. The game's timings are jittered (FR-044), so one episode is a
#: draw, and the noise in a mean falls with the square root of how many you take.
#:
#: **Twelve, and this is the lever a long night actually converts into quality.** The measured failure
#: is not too little search — it is selection deciding on noise: NEAT's winner pruned itself to six
#: connections because a deletion that costs real ability is invisible when the same network scores
#: 1,500 on one run and 3,100 on the next. Six episodes halve that spread against one; twelve halve it
#: again against six. Doubling the episodes doubles the cost of a generation and buys a signal that
#: selection can actually act on, which is the better trade whenever there are hours to spend.
EPISODES_PER_GENOME = 12

#: The curriculum of M6 §6. A stage ends when the best genome's fitness reaches `promote_at` or
#: when `generations` are spent, whichever comes first — the cap is there so that a stage which
#: turns out to be unlearnable cannot swallow the whole run in silence.
#:
#: The thresholds are read off the normal maze: 244 pellets, so clearing level 1 is worth 2,440 points
#: in the first two stages (a power pellet is an ordinary one there) and 2,600 in the third.
#:
#: Stage 1 keeps the "almost all of level 1" bar of 2,200, because nothing there can kill Pacman —
#: the ghosts are inert, so ending an episode at the first death changes nothing about it. Stages 2
#: and 3 have **one life** now, so a score that took three to reach is out of range: 1,200 is a
#: provisional bar for "gets most of the way through a level without dying", and the log is what will
#: say whether it was set anywhere near right.
#:
#: Stage 3's cap is high because time, not generations, is what a campaign budgets — a generation is
#: not a fixed amount of work, since a better agent lives longer — and `--max-seconds` is what
#: actually stops a run.
CURRICULUM = [
    {"stage": STAGE_MAZE_ONLY, "generations": 60, "promote_at": 2200.0, "what": "walk and eat"},
    {"stage": STAGE_GHOSTS, "generations": 200, "promote_at": 1200.0, "what": "stay alive"},
    {"stage": STAGE_FULL, "generations": 4000, "promote_at": None, "what": "the whole game"},
]

#: How much of a run's wall-clock budget each stage may spend, as a **cumulative** fraction of it.
#:
#: Without this a stage takes whatever it is given, and the two teaching stages will take all of it:
#: stage 2 promotes on a fitness bar it does not reliably clear, and its 200-generation cap is far
#: more than a short run can pay for. That is survivable overnight and fatal in an hour — the winner
#: comes out of stage 2, and stage 3 is the only stage a whole-game score is measured on.
#:
#: It can only ever *end* a stage early. A stage that promotes before its share is spent hands the
#: rest to the one after it, which is why the last entry is 1.0 and not a third: stage 3 gets
#: everything the teaching stages did not use.
STAGE_BUDGET_SHARE = {STAGE_MAZE_ONLY: 0.15, STAGE_GHOSTS: 0.45, STAGE_FULL: 1.0}


def stage_deadlines(started: float, max_seconds, stages) -> dict:
    """When each stage has to hand over, as `time.perf_counter` readings.

    A run with no budget has no deadlines, and a run of a single stage gives that stage all of it —
    `--stage 3` means "spend the budget on stage 3", not "spend 55 % of it".
    """
    if max_seconds is None:
        return {entry["stage"]: float("inf") for entry in stages}

    if len(stages) == 1:
        return {stages[0]["stage"]: started + max_seconds}

    return {entry["stage"]: started + (max_seconds * STAGE_BUDGET_SHARE[entry["stage"]])
            for entry in stages}

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

    Returns the *fitness* alongside the raw figures, so that what selection sees and what a log line
    reports come out of one place.
    """
    flat_net, seeds, stage, library_path, ends_at_first_death, ghost_bonus, level_bonus, danger_penalty = task
    env = _worker_env(len(seeds), library_path)
    env.set_episode_ends_at_first_death(ends_at_first_death)
    env.set_net(flat_net)
    scores, steps, levels, ghosts = env.run(seeds, stage, MAZE_NORMAL)

    # The two bonuses arrive in the task rather than being read from module scope, so that a run can
    # be told what to value without editing the trainer — and so that what a winner file records is
    # what that run actually optimised.
    danger = env.danger()

    fitness = sum(score + (ghost_bonus * eaten) + (level_bonus * (level - 1)) - (danger_penalty * near)
                  for score, eaten, level, near in zip(scores, ghosts, levels, danger)) / float(len(scores))

    return fitness, scores, steps, levels, ghosts


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

    def draw_seeds(self) -> list:
        """This generation's episodes.

        A fresh draw each generation, and the *same* draw for every genome in it: genomes then have
        to be comparable with each other, which is all selection needs, and nothing is comparable
        across generations — which is the right way round.
        """
        seeds = []

        while len(seeds) < self.arguments.episodes:
            candidate = self.random.randrange(1, 1_000_000)

            if (candidate not in ACCEPTANCE_SEEDS) and (candidate not in seeds):
                seeds.append(candidate)

        return seeds

    def evaluate(self, genomes, config) -> None:
        seeds = self.draw_seeds()
        started = time.perf_counter()

        flat = []
        for genome_id, genome in genomes:
            flat.append((net.from_genome(genome, config), seeds, self.stage, self.arguments.library,
                         self.arguments.episode == "one-life", self.arguments.ghost_bonus,
                         self.arguments.level_bonus, self.arguments.danger_penalty))

        if self.pool is not None:
            results = self.pool.map(_play, flat, chunksize=1)
        else:
            results = [_play(task) for task in flat]

        total_steps = 0
        best_index = 0

        for index, ((genome_id, genome), (fitness, scores, steps, levels, ghosts)) in enumerate(zip(genomes, results)):
            genome.fitness = fitness
            total_steps += sum(steps)

            if genome.fitness > genomes[best_index][1].fitness:
                best_index = index

        best_genome = genomes[best_index][1]
        _, best_scores, best_steps, best_levels, best_ghosts = results[best_index]
        elapsed = time.perf_counter() - started

        if best_genome.fitness > self.best_fitness:
            self.best_fitness = best_genome.fitness
            self.best_net = flat[best_index][0]
            self.best_report = {
                "stage": self.stage,
                "generation": self.generation,
                "fitness": best_genome.fitness,
                "mean_score": sum(best_scores) / float(len(best_scores)),
                "scores": best_scores,
                "ghosts_eaten": sum(best_ghosts),
                "level": max(best_levels),
                "decisions": sum(best_steps),
            }

            # Written the moment it improves, not at the end of the stage. A stage-3 run is hours,
            # and writing only at the end meant the file held the *previous* stage's winner for all
            # of it — so measuring or exporting mid-flight silently used the wrong network, and an
            # interrupted run kept nothing. A few kilobytes of JSON per improvement is nothing
            # against a generation.
            _write_winner(self.arguments.out, self.best_net, self.best_report, self.arguments)

        self.generation += 1

        print(
            f"  stage {self.stage} gen {self.generation:4d}  "
            f"best {best_genome.fitness:8.1f}  score {sum(best_scores) / len(best_scores):7.1f}  "
            f"ghosts {sum(best_ghosts):3d}  level {max(best_levels)}  "
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
        "training": {**report, "population": arguments.population_size, "maze": "normal",
                     "seed": arguments.seed, "episodes": arguments.episodes,
                     "ghost_bonus": arguments.ghost_bonus, "level_bonus": arguments.level_bonus,
                     "danger_penalty": arguments.danger_penalty,
                     "episode": arguments.episode,
                     # Shared with train_es.py, whose search has no deletion to forbid and
                     # therefore no such flag.
                     "no_deletion": getattr(arguments, "no_deletion", False)},
    }

    with open(path, "w") as handle:
        json.dump(payload, handle, indent=2)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stage", type=int, choices=[1, 2, 3], help="train one stage only")
    parser.add_argument("--generations", type=int, help="override the stage's generation cap")
    parser.add_argument("--workers", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--episodes", type=int, default=EPISODES_PER_GENOME,
                        help="episodes each genome is scored on per generation")
    parser.add_argument("--episode", choices=["one-life", "whole-run"], default="one-life",
                        help="stop an episode at the first death, or play the run out as a scored run does")
    parser.add_argument("--no-deletion", action="store_true",
                        help="forbid NEAT from removing nodes and connections: grow only")
    parser.add_argument("--max-seconds", type=float, default=None,
                        help="stop cleanly after this long, whatever generation it is on")
    parser.add_argument("--ghost-bonus", type=int, default=GHOST_BONUS,
                        help="fitness paid per ghost on top of the game's own score; 0 is the arcade")
    parser.add_argument("--level-bonus", type=int, default=0,
                        help="fitness paid per level finished, which the score only pays for indirectly")
    parser.add_argument("--danger-penalty", type=int, default=0,
                        help="fitness charged per decision taken within four cells of a killing ghost")
    parser.add_argument("--seed", type=int, default=1,
                        help="the run's own draw: NEAT's initial population and its mutations")
    parser.add_argument("--config", default=_DEFAULT_CONFIG)
    parser.add_argument("--out", default=os.path.join(_HERE, "winner.json"))
    parser.add_argument("--checkpoint-every", type=int, default=25, help="generations, 0 to switch it off")
    parser.add_argument(
        "--library",
        default=os.path.join(os.path.dirname(_HERE), "build-host", "libpacman_env.so"),
        help="the game as a shared library",
    )
    arguments = parser.parse_args(argv)

    # neat-python draws from the `random` module's global generator, so this is the whole of what
    # makes one run differ from another: the maze no longer varies and neither does the game. Two
    # runs of the same seed are the same run, which is what makes a comparison between two
    # configurations worth reading (FR-114 applied to the search rather than to an episode).
    random.seed(arguments.seed)

    config = neat.Config(
        neat.DefaultGenome, neat.DefaultReproduction, neat.DefaultSpeciesSet, neat.DefaultStagnation, arguments.config
    )
    arguments.population_size = config.pop_size

    if arguments.no_deletion:
        # Structural freedom upwards and none downwards. NEAT is allowed to add and forbidden to
        # remove, which tests the obvious reading of "give the search more freedom and more time"
        # directly: the measured collapse to six connections was deletion, and DEC-044 had already
        # halved these once. Set here rather than in a second config file, because a copy of 3.8 kB
        # of settings differing in two lines is two files to keep in step.
        config.genome_config.conn_delete_prob = 0.0
        config.genome_config.node_delete_prob = 0.0

        # The same door, by its other name. `net.py` flattens only the *enabled* connections, so a
        # connection that gets switched off is gone from the network that plays and from the network
        # that ships — deletion in everything but the word. At 1 % over 92 connections that is about
        # one a generation, which is why a run with deletion forbidden still came out of the night
        # holding 22 of its initial 92. Forbidding one and leaving the other is not an experiment.
        config.genome_config.enabled_mutate_rate = 0.0

        print("deletion is off: NEAT may add structure and may not remove or disable it", flush=True)

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

    def is_out_of_time() -> bool:
        """Whether the wall-clock budget is spent.

        A budget in *seconds* rather than in generations, because a generation is not a fixed
        amount of work: a better agent lives longer, so its episodes are longer and its generations
        slower. An overnight campaign has a morning to be finished by, not a generation count.
        """
        return (arguments.max_seconds is not None) and ((time.perf_counter() - started) >= arguments.max_seconds)

    deadlines = stage_deadlines(started, arguments.max_seconds, stages)

    try:
        for entry in stages:
            if is_out_of_time():
                break

            trainer.stage = entry["stage"]
            generations = arguments.generations or entry["generations"]
            promote_at = entry["promote_at"]
            deadline = deadlines[entry["stage"]]

            print(f"\n=== stage {entry['stage']}: {entry['what']} "
                  f"({generations} generations max, {arguments.workers} workers, "
                  f"{arguments.episode} episodes) ===", flush=True)

            # Run a generation at a time so that the stage can end the moment it is learned. `run`
            # would otherwise only come back when the whole cap is spent.
            for _ in range(generations):
                population.run(trainer.evaluate, 1)

                if (promote_at is not None) and (trainer.best_fitness >= promote_at):
                    print(f"  promoted: {trainer.best_fitness:.1f} >= {promote_at:.1f}", flush=True)
                    break

                if time.perf_counter() >= deadline:
                    print(f"  stage {entry['stage']} is out of time after "
                          f"{(time.perf_counter() - started) / 60.0:.0f} min of the run", flush=True)
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
