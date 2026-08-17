"""An unattended training campaign: several runs, one after another, and one summary to read.

    Training/.venv/bin/python Training/campaign.py --hours 1
    ./dev.sh train --hours 1                  # the same thing, detached, with the log followed

Runs each configuration below for its share of the wall-clock time it is given, measures the winner
against FR-037 on the **normal maze**, and appends a row to `Training/campaign/summary.md`. Read that
file when it is done; it is the whole point of this script.

Four things make it safe to leave alone.

**The budget is an input, and the runs share it.** `--hours` says when the campaign has to be
finished, and each run gets a slice in proportion to its `hours` below — so the same list of runs is
an hour at lunchtime or a night, without editing anything. What does *not* happen is a short campaign
quietly running the first configuration and skipping the rest, which is what a fixed slice per run
does to a budget that cannot pay for all of them.

**A run that cannot be paid for is dropped, not shortened.** Each run says the least time it is worth
starting with (`min_hours`), because a run too short to reach stage 3 does not produce a weaker
answer — it produces a stage-2 network, and stage 3 is the only stage FR-037 is about. Its share goes
to the runs that remain.

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

**A rebuild of `build-host` no longer reaches it.** The campaign copies `libpacman_env.so` into its
own output directory on the way in and hands every trainer and every measurement that copy, so the
game a campaign is run against is the game it started with. It used to load the live build through
ctypes, and that cost a run: a `git stash` during a campaign reverted `env_api.c`, an unrelated
`./dev.sh check` rebuilt the library from it, and the next process to start died on `undefined
symbol: env_ghosts_eaten`. The already-running workers kept their mapping and finished, which is what
made it look fine until the measurement. The snapshot is taken once per campaign and reused by a
resumed one, so carrying on after a reboot also carries on against the same game.

**The runs vary one thing each**, against a baseline that is already measured. They no longer vary the
seed: whether a given configuration got lucky is a question worth asking *after* one of them works,
and there are four more useful questions to ask first. What varies is the pruning, then the search,
then the objective, then the capacity — the list is above `RUNS`, next to the runs it describes.

The maze never varies; the AI only ever plays the normal one (FR-040). The game's *timings* do
(FR-044), which is why a genome is scored on twelve episodes rather than one.

See Docu/Design/M6-Pacman-AI.md §14.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_OUT_DIR = os.path.join(_HERE, "campaign")
_PYTHON = sys.executable

#: The game the trainers play, as it was when the campaign started. See the header: this is a copy
#: on purpose, so that building the host tree while a campaign runs is an ordinary thing to do.
_LIVE_LIBRARY = os.path.join(os.path.dirname(_HERE), "build-host", "libpacman_env.so")
_CAMPAIGN_LIBRARY = os.path.join(_OUT_DIR, "libpacman_env.so")

#: One entry per run. `args` goes to the trainer.
#:
#: `hours` is the run's **share** of the campaign, not an absolute: a campaign given three hours
#: divides them the way a campaign given twelve does, so the same list is a lunch break or a night.
#: The default budget is their sum, which is what these figures used to mean literally.
#:
#: `min_hours` is the least it is worth starting with. It is not a preference — below it the run
#: does not reach stage 3 in any useful number of generations, and stage 3 is the only stage FR-037
#: is measured on. Measured on four cores: a NEAT generation costs about 14 s (250 genomes x 12
#: episodes) and an ES one about 3 s (32 x 12), which is the whole reason their floors differ by
#: six times. `--episode whole-run` plays three lives instead of one, so its generations cost
#: roughly three times what the one-life ones do.
#:
#: **Every earlier result was measured against ghosts that no longer exist.** DEC-049 rolled the
#: ghosts back to the arcade's greedy one-cell rule, so a ghost walks into the wall between it and
#: its target instead of routing around it — a different game, and an easier one. The agent that ships
#: scored 3,035 against the route-searching ghosts and 2,706 against these. Nothing below is compared
#: against a figure taken before that change; the first run *is* the new baseline.
#:
#: Only two runs, because a campaign that varies four things over twelve hours gives each of them
#: three — and three hours of ES is where the sigma restarts have only just started working. Two
#: runs of six differ in exactly one thing:
#:
#:   arcade-danger        the best recipe there is, re-measured against the ghosts it will play
#:   arcade-danger-wide   the same, with 32 hidden units instead of 16
#:
#: What is *not* varied, and why — each was measured and is not worth another twelve hours. The
#: search: NEAT deletes structure whenever the fitness is noisy and FR-044's jitter is noise, so its
#: winner used 6 of 23 inputs (DEC-048). The objective: the ghost bonus cost score and more training
#: made it worse, a level bonus is identically zero until a level is finished so it has no gradient,
#: and a danger penalty of 25 scored 2,261 against 10's 3,035. The population: 64 was worse than 32
#: at equal wall-clock. What is left untested is capacity, and it is the one with a reason to be
#: retested now — a greedy ghost is a *predictable* ghost, and patterns worth learning are exactly
#: what a network runs out of room for.
#:
#: `trainer` says which script runs. Both take the same arguments for the things they share, because
#: they share the episode, the fitness and the curriculum — see train_es.py.
RUNS = [
    {
        "name": "arcade-danger",
        "trainer": "train_es.py",
        "hours": 6.0,
        "min_hours": 0.25,
        "args": ["--seed", "1", "--episode", "one-life", "--danger-penalty", "10"],
        "what": "the shipped recipe — 23-16-4, one life, ten points a dangerous decision",
    },
    {
        "name": "arcade-danger-wide",
        "trainer": "train_es.py",
        "hours": 6.0,
        "min_hours": 0.25,
        "args": ["--seed", "1", "--episode", "one-life", "--danger-penalty", "10", "--hidden", "32"],
        "what": "the same again with 32 hidden units, to see whether 16 was the ceiling",
    },
]

#: Held back from the runs so that one which overruns eats the spare rather than the next run's
#: slice. A **fraction** rather than the half hour it used to be, because that half hour was sized
#: against a night and is half of a one-hour campaign. `--max-seconds` stops between generations, so
#: the worst overrun is one generation per run: 5 % is three minutes of an hour and 36 of a night,
#: and both are the right order.
CAMPAIGN_SLACK_FRACTION = 0.05

#: What the whole campaign may take when nobody says. The runs' shares add up to it, so the default
#: campaign is the one these figures always described. `--hours`, or `CAMPAIGN_HOURS` in the
#: environment, is what makes it an hour instead.
DEFAULT_CAMPAIGN_HOURS = sum(run["hours"] for run in RUNS)

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


def _snapshot_library() -> bool:
    """Take the campaign's own copy of the game, or keep the one a previous start took.

    Kept rather than refreshed, and that is the whole point: a campaign resumed after a reboot is
    the *same* experiment as the one that was interrupted, so it has to play the same game — even
    if the tree has been built since. A fresh campaign is what `--fresh` gives, and that clears
    this directory with the winners.
    """
    if os.path.exists(_CAMPAIGN_LIBRARY):
        _log(f"playing the campaign's own copy of the game, taken "
             f"{time.strftime('%d %b %H:%M', time.localtime(os.path.getmtime(_CAMPAIGN_LIBRARY)))}")
        return True

    if not os.path.exists(_LIVE_LIBRARY):
        _log(f"{_LIVE_LIBRARY} does not exist, and every trainer loads it — build the host tree "
             f"first: ./dev.sh host")
        return False

    # copy2, so the snapshot carries the build's own timestamp and the line above says when the
    # game was built rather than when this campaign happened to start.
    shutil.copy2(_LIVE_LIBRARY, _CAMPAIGN_LIBRARY)
    _log(f"took a copy of the game to play against: {os.path.relpath(_CAMPAIGN_LIBRARY, _HERE)} — "
         f"building the host tree from here on does not reach this campaign")
    return True


def _evaluate(winner_path: str) -> "tuple[str, dict]":
    """Run evaluate.py against a winner and pull the two numbers out of what it printed.

    The text is kept as well as the numbers: it carries the episode's score and the baseline's
    spread, and a verdict without them cannot be argued with afterwards.
    """
    result = subprocess.run(
        [_PYTHON, os.path.join(_HERE, "evaluate.py"), "--winner", winner_path,
         "--library", _CAMPAIGN_LIBRARY],
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


def _write_summary(rows: list, dropped: list, hours: float) -> None:
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
        handle.write("| run | what | budget | score | vs. random | factor | nodes | conns | gen | FR-037 |\n")
        handle.write("|---|---|---|---|---|---|---|---|---|---|\n")

        for row in rows:
            factor = "—"
            if row["numbers"]["trained"] and row["numbers"]["random"]:
                factor = f"{row['numbers']['trained'] / row['numbers']['random']:.1f}x"

            def number(value):
                return f"{value:.1f}" if isinstance(value, float) else "—"

            budget = f"{row['budget_hours']:.2f} h" if row.get("budget_hours") else "—"

            handle.write(
                f"| `{row['name']}` | {row['what']} | {budget} | "
                f"{number(row['numbers']['trained'])} | {number(row['numbers']['random'])} | "
                f"{factor} | {row.get('nodes') or '—'} | {row.get('conns') or '—'} | "
                f"{row.get('generation') if row.get('generation') is not None else '—'} | "
                f"{'**met**' if row['numbers']['met'] else 'not met'} |\n"
            )

        if dropped:
            # In the committed record and not only in a log that is not. "Why is there one row when
            # four were configured" is the question this file exists to answer, and it has been
            # asked once already.
            handle.write(f"\nA budget of **{hours:.2f} h** could not pay for "
                         f"{len(dropped)} of the configured runs, so they were not started: "
                         + ", ".join(f"`{run['name']}` (needs {run['min_hours']:.2f} h)"
                                     for run in dropped)
                         + ". A run too short to reach stage 3 answers a different question,\n"
                           "so it is dropped rather than shortened.\n")

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


def _slices(runs: list, seconds: float) -> "tuple[dict, list]":
    """Share `seconds` out between `runs`, and say which of them the budget cannot pay for.

    Proportional to each run's `hours`, except that a run whose share falls below its `min_hours`
    is dropped and its time given to the others. Dropped rather than shortened on purpose: a run
    too short to reach stage 3 is not a weaker answer to the question, it is an answer to a
    different one, and it would sit in the summary looking like the first.

    The greediest run goes first, because dropping the one hardest to pay for is what most often
    pays for all the rest — an hour that cannot afford NEAT can afford three ES runs.
    """
    remaining = list(runs)

    while remaining:
        total = sum(run["hours"] for run in remaining)
        shares = {run["name"]: seconds * run["hours"] / total for run in remaining}
        short = [run for run in remaining if shares[run["name"]] < (run["min_hours"] * 3600.0)]

        if not short:
            return shares, [run for run in runs if run not in remaining]

        greediest = max(short, key=lambda run: run["min_hours"])
        remaining = [run for run in remaining if run is not greediest]

    return {}, list(runs)


def _budget_that_would_run(run: dict, runs: list) -> float:
    """The smallest campaign, in hours, that would actually start `run`.

    Not simply its `min_hours`: dropping the runs it cannot afford hands their time to the ones that
    remain, so a run needing a quarter of an hour can be paid for by a campaign of a third of one
    even though four runs are configured. Answering with the naive figure sent the reader to three
    times the budget they needed, which is a worse kind of wrong than being approximate.
    """
    hours = run["min_hours"]

    for _ in range(64):
        shares, _dropped = _slices(runs, hours * 3600.0 * (1.0 - CAMPAIGN_SLACK_FRACTION))

        if run["name"] in shares:
            return hours

        hours *= 1.1

    return hours


def main(argv: "list[str]") -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--hours", type=float,
                        default=float(os.environ.get("CAMPAIGN_HOURS", DEFAULT_CAMPAIGN_HOURS)),
                        help="when the whole campaign has to be finished; the runs share it")
    arguments = parser.parse_args(argv)

    os.makedirs(_OUT_DIR, exist_ok=True)

    if not _snapshot_library():
        return 2

    deadline = time.monotonic() + (arguments.hours * 3600.0)
    rows = []

    # A run whose winner is already on disk is measured and not repeated, and measuring costs a
    # second — so it is kept out of the sharing rather than given a slice it will never spend. That
    # is what makes a resumed campaign put its whole budget into what is actually left to do.
    to_train = [run for run in RUNS if not os.path.exists(os.path.join(_OUT_DIR, f"{run['name']}.json"))]
    shares, dropped = _slices(to_train, arguments.hours * 3600.0 * (1.0 - CAMPAIGN_SLACK_FRACTION))

    # Said at the start, because the one question anybody has about a campaign is when it will be
    # done, and it is answerable from the configuration rather than by watching.
    _log(f"{len(shares)} run(s) to train, {arguments.hours:.2f} h ceiling — expect to be finished "
         f"around {time.strftime('%H:%M', time.localtime(time.time() + (arguments.hours * 3600.0)))}")

    for run in dropped:
        # Loudly, and with the number that would change it. A campaign that silently ran fewer runs
        # than it was configured with is how a night turns into a single row nobody expected.
        _log(f"dropping {run['name']}: {arguments.hours:.2f} h cannot give it the "
             f"{run['min_hours']:.2f} h it needs — --hours "
             f"{_budget_that_would_run(run, to_train):.1f} would")

    if not shares:
        _log("nothing left to train — measuring what is on disk")

    for baseline in BASELINES:
        if not os.path.exists(baseline["path"]):
            continue

        _log(f"measuring the baseline: {baseline['name']}")
        text, numbers = _evaluate(baseline["path"])
        rows.append({**baseline, **_describe(baseline["path"]), "text": text, "numbers": numbers})
        _write_summary(rows, dropped, arguments.hours)

    for run in RUNS:
        winner = os.path.join(_OUT_DIR, f"{run['name']}.json")
        log_path = os.path.join(_OUT_DIR, f"{run['name']}.log")

        # What this run was actually given, which the summary reports. Two runs of the same
        # configuration are only comparable if it is on the page: 20 minutes and 3 hours produce
        # rows that otherwise look alike.
        trained_hours = None

        if os.path.exists(winner):
            _log(f"{run['name']} already has a winner — measuring it, not retraining")
        elif run["name"] not in shares:
            continue
        else:
            remaining = deadline - time.monotonic()

            if remaining <= 300.0:
                _log(f"skipping {run['name']}: only {remaining / 60.0:.0f} min of the campaign left")
                continue

            # The share, or whatever is left of the campaign — whichever is smaller. A run that
            # overran must not be paid for by the run after it.
            seconds = min(shares[run["name"]], remaining - 60.0)
            trained_hours = seconds / 3600.0
            worker_arguments = ["--workers", WORKERS] if WORKERS else []
            trainer = run.get("trainer", "train.py")

            _log(f"training {run['name']} with {trainer} for {seconds / 3600.0:.2f} h -> {log_path}")

            # `--checkpoint-every` is NEAT's, and only NEAT's: the evolution strategy has no
            # population to pickle, its whole state is a mean and a deviation.
            trainer_arguments = ["--checkpoint-every", "0"] if trainer == "train.py" else []

            with open(log_path, "w") as handle:
                result = subprocess.run(
                    [_PYTHON, "-u", os.path.join(_HERE, trainer),
                     "--out", winner, "--max-seconds", str(seconds),
                     "--library", _CAMPAIGN_LIBRARY,
                     *trainer_arguments, *worker_arguments, *run["args"]],
                    stdout=handle,
                    stderr=subprocess.STDOUT,
                )

            # A trainer that died is not a trainer that found nothing, and the difference decides
            # what to do next. It used to be invisible: three runs crashed on their first line one
            # night, each took a second to do it, and the campaign reported them exactly the way it
            # reports a run that trained for three hours and lost. The last lines of the log are
            # quoted here so the reason is in front of whoever reads the campaign log.
            if result.returncode != 0:
                _log(f"{run['name']} FAILED: {trainer} exited {result.returncode} — {log_path} ends:")

                with open(log_path) as handle:
                    for line in handle.read().splitlines()[-5:]:
                        _log(f"  | {line}")

        if not os.path.exists(winner):
            _log(f"{run['name']} produced no winner — see {log_path}")
            continue

        _log(f"measuring {run['name']}")
        text, numbers = _evaluate(winner)
        rows.append({**run, **_describe(winner), "text": text, "numbers": numbers,
                     "budget_hours": trained_hours})

        # Written after every run rather than at the end, so an interrupted campaign still has a
        # readable summary of what it did manage.
        _write_summary(rows, dropped, arguments.hours)

    _log("campaign finished")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
