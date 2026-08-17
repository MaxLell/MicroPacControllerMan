#!/usr/bin/env python3
"""Fit the look-ahead player's six evaluation weights against the score it plays for.

A (1+lambda) evolution strategy with per-parameter step sizes: hold the best weights found, draw
`lambda` mutations of them, keep the best if it beats the incumbent, and shrink the steps when a
generation brings nothing. Six parameters is a small enough space that nothing cleverer is needed,
and each candidate costs whole games, so the wall clock goes into playing rather than into the
search over weights.

**The fitness is deterministic**, which is worth knowing before reading a generation: the seeds are
fixed, so the same weights always score the same and there is no measurement noise to average out.
What there is instead is the risk of fitting the sixteen draws rather than the game, which is why
the seeds start at 2000 and the set 1000..1019 is never trained against — it is what a result gets
validated on afterwards.

Writes the best weights it has to `lookahead_weights.json` after every generation, so the run is
useful whenever it is stopped. It does not touch the firmware: adopting a result means copying the
numbers into `pacman_lookahead.c`'s defaults deliberately.

    cmake --build build-host -j --target pacman_lookahead_fitness
    FIT_HOURS=1.5 python3 Training/fit_lookahead.py
"""
import json
import os
import random
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
BINARY = os.path.join(HERE, os.pardir, "build-host", "pacman_lookahead_fitness")
OUT = os.path.join(HERE, "lookahead_weights.json")
LOG = os.path.join(HERE, "fit_lookahead.log")

NAMES = ["point", "death", "threat", "prey", "food", "escape"]
START = [2, 70791, 13, 53, 2, 10]   # what the firmware ships; a fit starts from it, not from nothing

# How far each may move at the start, and the floor it may not fall below. `death` is deliberately
# coarse: it only has to stay far larger than anything a branch can gain.
STEP = [2, 15000, 4, 15, 3, 4]
LOW = [0, 1000, 0, 0, 0, 0]
HIGH = [200, 2000000, 200, 400, 200, 200]

FIRST_SEED = 2000
RUNS = int(os.environ.get("FIT_RUNS", "12"))
LAMBDA = int(os.environ.get("FIT_LAMBDA", "12"))
BUDGET_S = float(os.environ.get("FIT_HOURS", "1.4")) * 3600.0


def score(weights):
    """Mean score of these weights over the training seeds; -1 if the run failed."""
    args = [BINARY, str(FIRST_SEED), str(RUNS)] + [str(int(w)) for w in weights]
    try:
        out = subprocess.run(args, capture_output=True, text=True, timeout=1800)
    except subprocess.TimeoutExpired:
        return -1.0, 0.0
    if out.returncode != 0:
        return -1.0, 0.0
    # The first two fields and no more: `fit_lookahead.c` also names the maze it played, and unpacking
    # the whole line is what killed a ten-hour run fifteen minutes into it.
    fields = out.stdout.split()

    if len(fields) < 2:
        return -1.0, 0.0

    return float(fields[0]), float(fields[1])


def mutate(weights, steps, rng):
    out = []
    for value, step, low, high in zip(weights, steps, LOW, HIGH):
        moved = value + rng.gauss(0.0, step)
        out.append(int(max(low, min(high, round(moved)))))
    return out


def main():
    rng = random.Random(20260817)
    best = list(START)
    steps = list(STEP)

    log = open(LOG, "a", buffering=1)

    def say(text):
        stamp = time.strftime("%H:%M:%S")
        log.write(f"[{stamp}] {text}\n")
        print(f"[{stamp}] {text}", flush=True)

    started = time.time()
    say(f"fitting {len(NAMES)} weights, {RUNS} runs a candidate, lambda {LAMBDA}, "
        f"budget {BUDGET_S/3600:.2f} h, seeds {FIRST_SEED}..{FIRST_SEED+RUNS-1}")

    best_score, best_levels = score(best)
    say(f"incumbent (the hand-picked defaults): {best_score:.0f}, level {best_levels:.2f}")

    generation = 0
    workers = max(1, (os.cpu_count() or 2) - 1)

    while time.time() - started < BUDGET_S:
        generation += 1
        candidates = [mutate(best, steps, rng) for _ in range(LAMBDA)]

        with ProcessPoolExecutor(max_workers=workers) as pool:
            results = list(pool.map(score, candidates))

        ranked = sorted(zip(results, candidates), key=lambda pair: pair[0][0], reverse=True)
        (top_score, top_levels), top = ranked[0]

        if top_score > best_score:
            gain = top_score - best_score
            best, best_score, best_levels = top, top_score, top_levels
            say(f"gen {generation}: {best_score:.0f} (+{gain:.0f}), level {best_levels:.2f}  "
                + " ".join(f"{n}={v}" for n, v in zip(NAMES, best)))
            json.dump({"weights": dict(zip(NAMES, best)), "score": best_score,
                       "levels": best_levels, "runs": RUNS, "first_seed": FIRST_SEED,
                       "generation": generation},
                      open(OUT, "w"), indent=2)
        else:
            # Nothing better this round: look in a smaller neighbourhood rather than wander.
            steps = [max(1.0, s * 0.85) for s in steps]
            say(f"gen {generation}: nothing better (best candidate {top_score:.0f}), "
                f"steps now {[round(s, 1) for s in steps]}")

    say(f"done after {generation} generations: {best_score:.0f} with "
        + " ".join(f"{n}={v}" for n, v in zip(NAMES, best)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
