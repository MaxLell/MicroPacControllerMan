"""The training environment, as Python sees it.

A ctypes wrapper over ``libpacman_env.so`` — which is the firmware's own game compiled for the
host (FR-112). There is deliberately no game logic here and no re-implementation of the
observation: everything this module knows, it asks the library for, because a second copy of the
rules or of the feature layout is the first thing to diverge silently from what the target runs.

Every call is batched. One ``step`` advances every environment, because at a population of 150
the per-call cost across the language boundary would otherwise outweigh the simulation.

See Docu/Design/M6-Pacman-AI.md §7.
"""

import ctypes
import os
from typing import List, Sequence

_DEFAULT_LIBRARY = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build-host", "libpacman_env.so"
)

STAGE_MAZE_ONLY = 1
STAGE_GHOSTS = 2
STAGE_FULL = 3

#: Which maze a run is played on. The AI may only be handed control in the normal one (FR-040), so
#: that is where training and FR-037 live and the seeds are then ignored. ``MAZE_GENERATED`` is kept
#: so the question "how does this agent do on a maze nobody has played" can still be asked.
MAZE_NORMAL = 0
MAZE_GENERATED = 1


class PacmanEnv:
    """A batch of independent Pacman games, stepped together and never rendered."""

    def __init__(self, count: int, library_path: str = _DEFAULT_LIBRARY):
        if count < 1:
            raise ValueError("a batch needs at least one game")

        if not os.path.exists(library_path):
            raise FileNotFoundError(
                f"{library_path} is missing — build it with:\n"
                "  cmake -B build-host -DPACMAN_HOST_BUILD=ON -G 'Unix Makefiles'\n"
                "  cmake --build build-host -j"
            )

        self._lib = ctypes.CDLL(library_path)
        self._declare()

        self.count = count
        self.feature_count = int(self._lib.env_feature_count())
        self.action_count = int(self._lib.env_action_count())
        self.idle_limit_ms = int(self._lib.env_idle_limit_ms())
        self.max_decisions = int(self._lib.env_max_decisions())

        self._batch = self._lib.env_create(ctypes.c_uint32(count))
        if not self._batch:
            raise MemoryError("env_create failed")

        self._features = (ctypes.c_float * (count * self.feature_count))()
        self._reward = (ctypes.c_float * count)()
        self._done = (ctypes.c_uint8 * count)()
        self._actions = (ctypes.c_uint8 * count)()
        self._seeds = (ctypes.c_uint32 * count)()
        self._scores = (ctypes.c_uint32 * count)()
        self._steps = (ctypes.c_uint32 * count)()
        self._levels = (ctypes.c_uint8 * count)()

    def _declare(self) -> None:
        lib = self._lib
        lib.env_feature_count.restype = ctypes.c_uint32
        lib.env_action_count.restype = ctypes.c_uint32
        lib.env_idle_limit_ms.restype = ctypes.c_uint32
        lib.env_max_decisions.restype = ctypes.c_uint32
        lib.env_create.restype = ctypes.c_void_p
        lib.env_create.argtypes = [ctypes.c_uint32]
        lib.env_destroy.argtypes = [ctypes.c_void_p]
        lib.env_reset.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_uint8,
            ctypes.c_uint8,
        ]
        lib.env_observe.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float)]
        lib.env_step.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.POINTER(ctypes.c_float),
            ctypes.POINTER(ctypes.c_uint8),
        ]
        lib.env_scores.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
        lib.env_levels.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8)]
        lib.env_set_net.restype = ctypes.c_bool
        lib.env_set_net.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.POINTER(ctypes.c_uint16),
            ctypes.POINTER(ctypes.c_float),
            ctypes.POINTER(ctypes.c_uint16),
            ctypes.POINTER(ctypes.c_uint16),
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_uint32,
        ]
        lib.env_use_random_policy.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        lib.env_run.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_uint8,
            ctypes.c_uint8,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.POINTER(ctypes.c_uint8),
        ]

    def _load_seeds(self, seeds: Sequence[int], maze: int) -> None:
        """Hand the seeds over, or as many zeros as the library expects for the normal maze.

        ``MAZE_NORMAL`` ignores them, so a caller playing it may pass none at all rather than
        inventing numbers that mean nothing.
        """
        if maze == MAZE_NORMAL:
            seeds = seeds or [0] * self.count

        if len(seeds) != self.count:
            raise ValueError(f"expected {self.count} seeds, got {len(seeds)}")

        for index, seed in enumerate(seeds):
            self._seeds[index] = seed & 0xFFFFFFFF

    def reset(self, seeds: Sequence[int] = (), stage: int = STAGE_FULL, maze: int = MAZE_NORMAL) -> None:
        self._load_seeds(seeds, maze)

        self._lib.env_reset(self._batch, self._seeds, ctypes.c_uint8(stage), ctypes.c_uint8(maze))

    def observe(self) -> List[List[float]]:
        """One feature vector per environment."""
        self._lib.env_observe(self._batch, self._features)
        width = self.feature_count

        return [list(self._features[index * width : (index + 1) * width]) for index in range(self.count)]

    def step(self, actions: Sequence[int]):
        """Advance every environment to its next decision.

        Returns ``(rewards, dones)`` — points gained by each game during this step, and which
        episodes have ended.
        """
        if len(actions) != self.count:
            raise ValueError(f"expected {self.count} actions, got {len(actions)}")

        for index, action in enumerate(actions):
            self._actions[index] = action % self.action_count

        self._lib.env_step(self._batch, self._actions, self._reward, self._done)

        return list(self._reward), [bool(value) for value in self._done]

    def set_net(self, net) -> None:
        """Install a :class:`net.Net` as the policy every game plays with.

        The library copies the arrays and checks the topology with the firmware's own
        ``neural_net_is_well_formed``, so a genome the board could not evaluate raises here rather
        than being given a fitness.
        """
        accepted = self._lib.env_set_net(
            self._batch,
            ctypes.c_uint16(net.input_count),
            ctypes.c_uint16(net.node_count),
            ctypes.c_uint16(net.output_count),
            (ctypes.c_uint16 * len(net.output_nodes))(*net.output_nodes),
            (ctypes.c_float * len(net.biases))(*net.biases),
            (ctypes.c_uint16 * len(net.connection_offsets))(*net.connection_offsets),
            (ctypes.c_uint16 * len(net.connection_sources))(*net.connection_sources),
            (ctypes.c_float * len(net.connection_weights))(*net.connection_weights),
            ctypes.c_uint32(net.connection_count),
        )

        if not accepted:
            raise ValueError(
                f"the library refused the network ({net.node_count} nodes, "
                f"{net.connection_count} connections) — see neural_net_is_well_formed"
            )

    def use_random_policy(self, rng_seed: int = 1) -> None:
        """Play uniformly at random — the baseline FR-037 is measured against."""
        self._lib.env_use_random_policy(self._batch, ctypes.c_uint32(rng_seed & 0xFFFFFFFF))

    def run(self, seeds: Sequence[int] = (), stage: int = STAGE_FULL, maze: int = MAZE_NORMAL):
        """Play every game to its end with the installed policy.

        Returns ``(scores, steps, levels)``. This is the call training spends its time in: the
        observation, the network and the choice of action are all C, so a whole episode costs one
        crossing of the language boundary rather than one per decision.
        """
        self._load_seeds(seeds, maze)

        self._lib.env_run(
            self._batch,
            self._seeds,
            ctypes.c_uint8(stage),
            ctypes.c_uint8(maze),
            self._scores,
            self._steps,
            self._levels,
        )

        return list(self._scores), list(self._steps), list(self._levels)

    def scores(self) -> List[int]:
        self._lib.env_scores(self._batch, self._scores)

        return list(self._scores)

    def levels(self) -> List[int]:
        self._lib.env_levels(self._batch, self._levels)

        return list(self._levels)

    def close(self) -> None:
        if getattr(self, "_batch", None):
            self._lib.env_destroy(self._batch)
            self._batch = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        self.close()
