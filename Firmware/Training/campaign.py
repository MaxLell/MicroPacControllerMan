"""An unattended training campaign: several runs, one after another, and one summary to read.

    nohup Training/.venv/bin/python -u Training/campaign.py > Training/campaign/campaign.log 2>&1 &

Runs each configuration below for its slice of wall-clock time, measures the winner against FR-037
on the **normal maze**, and appends a row to `Training/campaign/summary.md`. Read that file when it
is done; it is the whole point of this script.

Three things make it safe to leave alone.

**Every run is time-budgeted, not generation-budgeted.** A generation is not a fixed amount of
work — a better agent lives longer, so its episodes are longer and its generations slower — and a
campaign has a time to be finished by rather than a generation count to reach. `train.py
--max-seconds` stops cleanly on its own, so no signals are involved. (Signals were tried and are a
trap: `pkill -f train.py` matches the pool workers as well as the parent, kills them, and leaves the
parent wedged in `pool.map` waiting for results that will never come.)

**Nothing is overwritten and nothing is lost.** Each run writes its own winner file, and `train.py`
writes it on every improvement rather than at the end, so even a hard kill leaves the best network
so far on disk.

**It is resumable.** A run whose winner file already exists is skipped, so if the machine is
rebooted the campaign can simply be started again and will carry on where it left off.

**One thing it is not safe against: rebuilding `build-host` while it runs.** The trainer loads
`libpacman_env.so` through ctypes, so replacing that file under a running campaign is replacing the
game mid-experiment. It has already cost one run: a `git stash` during a campaign reverted
`env_api.c`, an unrelated `./dev.sh check` rebuilt the library from it, and the next process to start
died on `undefined symbol: env_ghosts_eaten`. The already-running workers kept their mapping and
finished, which is what made it look fine until the measurement.

**The runs vary one thing each**, against a baseline that is already measured. They no longer vary the
seed: whether a given configuration got lucky is a question worth asking *after* one of them works,
and there are four more useful questions to ask first. What varies is the pruning, then the search,
then the objective, then the capacity — the list is above `RUNS`, next to the runs it describes.

The maze never varies; the AI only ever plays the normal one (FR-040). The game's *timings* do
(FR-044), which is why a genome is scored on twelve episodes rather than one.

See Docu/Design/M6-Pacman-AI.md §14.
"""

import json
import os
import subprocess
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_OUT_DIR = os.path.join(_HERE, "campaign")
_PYTHON = sys.executable

#: One entry per run. `hours` is that run's slice; `args` goes to the trainer. The ceiling below is
#: derived from these, so `hours` is the only thing to edit for a longer or shorter campaign.
#:
#: One variable at a time, against one baseline that is **already measured** and costs no time to
#: reuse: NEAT, the one-life objective, deletion on — 1,746 points, with a winner that used 6
#: connections against 23 inputs.
#:
#:   neat-no-deletion   against that baseline, only the pruning changed
#:   es-one-life        against that baseline, only the search changed
#:   es-whole-run       against es-one-life, only the objective changed
#:   es-whole-run-wide  against es-whole-run, only the capacity changed
#:
#: `neat-no-deletion` is here because it is the literal reading of "give the search more freedom and
#: more time": NEAT may add structure and may not remove it. The measured collapse was deletion, and
#: DEC-044 had already halved these probabilities once; zero is where that road ends. Whether
#: unchecked growth helps is genuinely unknown — a network that only ever grows can bloat without
#: playing better — which is why it is a run and not a decision.
#:
#: `trainer` says which script runs. Both take the same arguments for the things they share, because
#: they share the episode, the fitness and the curriculum — see train_es.py.
RUNS = [
    {
        "name": "neat-no-deletion",
        "trainer": "train.py",
        "hours": 3.0,
        "args": ["--seed", "1", "--episode", "one-life", "--no-deletion"],
        "what": "NEAT, but forbidden to remove nodes or connections",
    },
    {
        "name": "es-one-life",
        "trainer": "train_es.py",
        "hours": 3.0,
        "args": ["--seed", "1", "--episode", "one-life"],
        "what": "fixed 23-16-4 network, an episode ends at the first death",
    },
    {
        "name": "es-whole-run",
        "trainer": "train_es.py",
        "hours": 3.0,
        "args": ["--seed", "1", "--episode", "whole-run"],
        "what": "the same, but an episode is the three-life run FR-037 measures",
    },
    {
        "name": "es-whole-run-wide",
        "trainer": "train_es.py",
        "hours": 3.0,
        "args": ["--seed", "1", "--episode", "whole-run", "--hidden", "32"],
        "what": "the same again with 32 hidden units, to see whether 16 was the ceiling",
    },
]

#: Spare on top of the runs' own budgets, so that a run which overruns eats it rather than the next
#: run's slice. Half an hour is generous: `train.py --max-seconds` stops between generations, so the
#: worst overrun is one generation.
CAMPAIGN_SLACK_HOURS = 0.5

#: What the whole campaign may take. **Derived**, not written down, because it used to be both: a
#: ceiling of its own that had to be dragged along whenever a run's `hours` changed, and silently
#: clamped the runs when it was not — the second run simply did not happen. Now there is one place to
#: edit, which is `hours` above. `CAMPAIGN_HOURS` in the environment still overrides it, for "be done
#: by breakfast whatever the runs say".
CAMPAIGN_HOURS = float(os.environ.get("CAMPAIGN_HOURS", sum(run["hours"] for run in RUNS) + CAMPAIGN_SLACK_HOURS))

#: How many cores to evolve on. Unset means every one `train.py` can see, which is what you want on a
#: machine whose whole job this is — a container on a big host sees all of them. Set `WORKERS` to
#: fewer when the machine is also being *used*: three of four cores keeps a desktop responsive, and
#: that figure used to be hard-coded here, which quietly capped a sixteen-core machine at three.
WORKERS = os.environ.get("WORKERS")

#: Measured before the campaign and carried into the summary so the comparison is on one page. It was
#: trained on *generated* mazes, which is what makes it the right reference: it says what the change
#: of training maze is worth.
BASELINES = [
    {
        "name": "shipped",
        "path": os.path.join(_HERE, "winner.json"),
        "what": "the deterministic-game winner the firmware ships today — 4,980 on one fixed episode",
    },
]


def _log(message: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {message}", flush=True)


def _evaluate(winner_path: str) -> "tuple[str, dict]":
    """Run evaluate.py against a winner and pull the two numbers out of what it printed.

    The text is kept as well as the numbers: it carries the episode's score and the baseline's
    spread, and a verdict without them cannot be argued with afterwards.
    """
    result = subprocess.run(
        [_PYTHON, os.path.join(_HERE, "evaluate.py"), "--winner", winner_path],
        capture_output=True,
        text=True,
    )
    text = result.stdout + result.stderr
    numbers = {"trained": None, "random": None, "met": result.returncode == 0}

    # Parsed off evaluate.py's own verdict lines rather than recomputed, so the summary cannot
    # disagree with the harness that produced it:
    #
    #   FR-037: 2400.0 vs. required 4600 — NOT met
    #            2400.0 vs. random 433.5 (5.5x) — met
    for line in text.splitlines():
        stripped = line.strip()

        if stripped.startswith("FR-037:"):
            numbers["trained"] = float(stripped.split()[1])
        elif " vs. random " in stripped:
            numbers["random"] = float(stripped.split(" vs. random ")[1].split()[0])

    return text, numbers


def _write_summary(rows: list) -> None:
    os.makedirs(_OUT_DIR, exist_ok=True)
    path = os.path.join(_OUT_DIR, "summary.md")

    with open(path, "w") as handle:
        handle.write("# Training campaign\n\n")
        handle.write("FR-037 asks for **4,600** points on the normal maze — the only maze the AI\n")
        handle.write("may be handed control in (FR-040) — and for more than a uniform-random policy\n")
        handle.write("on the same maze. Both figures are the mean of twenty runs on the same twenty\n")
        handle.write("draws: the game's timings are jittered (FR-044), so a run is a draw rather than\n")
        handle.write("a measurement. Every figure below comes out of `Training/evaluate.py`, which\n")
        handle.write("measures both policies in one run so the comparison cannot drift.\n\n")
        handle.write("| run | what | score | vs. random | factor | nodes | conns | gen | FR-037 |\n")
        handle.write("|---|---|---|---|---|---|---|---|---|\n")

        for row in rows:
            factor = "—"
            if row["numbers"]["trained"] and row["numbers"]["random"]:
                factor = f"{row['numbers']['trained'] / row['numbers']['random']:.1f}x"

            def number(value):
                return f"{value:.1f}" if isinstance(value, float) else "—"

            handle.write(
                f"| `{row['name']}` | {row['what']} | "
                f"{number(row['numbers']['trained'])} | {number(row['numbers']['random'])} | "
                f"{factor} | {row.get('nodes') or '—'} | {row.get('conns') or '—'} | "
                f"{row.get('generation') if row.get('generation') is not None else '—'} | "
                f"{'**met**' if row['numbers']['met'] else 'not met'} |\n"
            )

        handle.write("\n## What each run reported\n")
        for row in rows:
            handle.write(f"\n### {row['name']} — {row['what']}\n\n```\n{row['text'].strip()}\n```\n")

        handle.write(
            "\n## Next\n\n"
            "If the best of these is still short of 4,600, the order to look in is the one\n"
            "[M6 §14](../../Docu/Design/M6-Pacman-AI.md) sets out: the 23 features first, then the\n"
            "expectimax reference agent as a teacher. Not the threshold — that is the owner's to\n"
            "move.\n"
        )

    _log(f"summary written to {path}")


def _describe(winner_path: str) -> dict:
    """The shape of a winner and where it came from, for the summary's columns."""
    try:
        with open(winner_path) as handle:
            payload = json.load(handle)
    except (OSError, ValueError):
        return {}

    return {
        "nodes": payload.get("node_count"),
        "conns": len(payload.get("connection_sources", [])),
        "generation": payload.get("training", {}).get("generation"),
    }


def main() -> int:
    os.makedirs(_OUT_DIR, exist_ok=True)
    deadline = time.monotonic() + (CAMPAIGN_HOURS * 3600.0)
    rows = []

    # Said at the start, because the one question anybody has about a campaign is when it will be
    # done, and it is answerable from the configuration rather than by watching.
    _log(f"{len(RUNS)} run(s), {sum(run['hours'] for run in RUNS):.2f} h of training, "
         f"{CAMPAIGN_HOURS:.2f} h ceiling — expect to be finished around "
         f"{time.strftime('%H:%M', time.localtime(time.time() + (CAMPAIGN_HOURS * 3600.0)))}")

    for baseline in BASELINES:
        if not os.path.exists(baseline["path"]):
            continue

        _log(f"measuring the baseline: {baseline['name']}")
        text, numbers = _evaluate(baseline["path"])
        rows.append({**baseline, **_describe(baseline["path"]), "text": text, "numbers": numbers})
        _write_summary(rows)

    for run in RUNS:
        winner = os.path.join(_OUT_DIR, f"{run['name']}.json")
        log_path = os.path.join(_OUT_DIR, f"{run['name']}.log")
        remaining = deadline - time.monotonic()

        if remaining <= 300.0:
            _log(f"skipping {run['name']}: only {remaining / 60.0:.0f} min of the campaign left")
            continue

        # The slice, or whatever is left of the campaign — whichever is smaller. A run that
        # overran must not be paid for by the run after it.
        seconds = min(run["hours"] * 3600.0, remaining - 60.0)

        if os.path.exists(winner):
            _log(f"{run['name']} already has a winner — measuring it, not retraining")
        else:
            worker_arguments = ["--workers", WORKERS] if WORKERS else []
            trainer = run.get("trainer", "train.py")

            _log(f"training {run['name']} with {trainer} for {seconds / 3600.0:.1f} h -> {log_path}")

            # `--checkpoint-every` is NEAT's, and only NEAT's: the evolution strategy has no
            # population to pickle, its whole state is a mean and a deviation.
            trainer_arguments = ["--checkpoint-every", "0"] if trainer == "train.py" else []

            with open(log_path, "w") as handle:
                subprocess.run(
                    [_PYTHON, "-u", os.path.join(_HERE, trainer),
                     "--out", winner, "--max-seconds", str(seconds),
                     *trainer_arguments, *worker_arguments, *run["args"]],
                    stdout=handle,
                    stderr=subprocess.STDOUT,
                )

        if not os.path.exists(winner):
            _log(f"{run['name']} produced no winner — see {log_path}")
            continue

        _log(f"measuring {run['name']}")
        text, numbers = _evaluate(winner)
        rows.append({**run, **_describe(winner), "text": text, "numbers": numbers})

        # Written after every run rather than at the end, so an interrupted campaign still has a
        # readable summary of what it did manage.
        _write_summary(rows)

    _log("campaign finished")

    return 0


if __name__ == "__main__":
    sys.exit(main())
