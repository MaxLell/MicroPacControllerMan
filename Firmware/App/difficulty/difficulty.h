/*
 * difficulty.h
 *
 * What a level number means: how fast everyone moves, how long a power pellet lasts,
 * when Blinky turns into Cruise Elroy, and how scatter gives way to chase
 * ([10 §10.9](../../../Docu/PrePlanning/10-Pacman-Game-Design.md), FR-025/026).
 *
 * This is the whole of the game's progression, in one lookup with no state. The maze
 * never changes (§10.2) — the arcade's did not either — so *this* table is what makes
 * level 15 harder than level 1, and having it in one place is what lets it be checked
 * against the source it was transcribed from rather than felt out by playing.
 *
 * **The table inside is the arcade's, in the arcade's own units**: percentages of full
 * speed, dot counts, seconds. Callers get milliseconds per cell, because that is what a
 * game loop can use. The conversion happens here precisely so the transcription can stay
 * literal and reviewable line by line against the Pac-Man Dossier.
 */

#ifndef DIFFICULTY_H
#define DIFFICULTY_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * difficulty - public types
 * ========================================================================= */

/*! \brief Levels are 1-based. */
#define DIFFICULTY_FIRST_LEVEL (1U)

/*! \brief The last level of a run (FR-027).
 *
 * The arcade has no last level; it has a level *21* after which nothing gets harder ever
 * again, and then runs until the player is out of lives. 21 is therefore the honest place
 * to put a finish line: clearing it means the whole difficulty curve has been walked, and
 * anything past it would be the same level with a different number on it.
 */
#define DIFFICULTY_FINAL_LEVEL (21U)

/*! \brief The most phases any scatter/chase plan has. */
#define DIFFICULTY_PHASE_MAX   (7U)

/*! \brief Everything a level changes, in units the game loop can use directly.
 *
 * Every `_period_ms` is milliseconds to cross one cell — smaller is faster. A period of
 * zero means "this case cannot arise at this level", which happens only to the frightened
 * columns on the levels where a power pellet no longer frightens anyone.
 */
typedef struct
{
    uint32_t pacman_period_ms;            /*!< Moving through an empty corridor  */
    uint32_t pacman_eating_period_ms;     /*!< Slower, the step after a pellet   */
    uint32_t pacman_frightened_period_ms; /*!< Faster while the ghosts are blue  */
    uint32_t pacman_frightened_eating_period_ms;

    uint32_t ghost_period_ms;
    uint32_t ghost_tunnel_period_ms;     /*!< A crawl; the one place to shake one off */
    uint32_t ghost_frightened_period_ms; /*!< Blue ghosts are slow, not just harmless */

    /*! \brief Cruise Elroy: Blinky speeds up as the maze empties (§10.9).
     *
     * Stage 1 wakes when this many pellets are left, stage 2 at the lower count. The
     * thresholds are absolute pellet counts, as the arcade states them. */
    uint16_t elroy1_pellets_left;
    uint32_t elroy1_period_ms;
    uint16_t elroy2_pellets_left;
    uint32_t elroy2_period_ms; /*!< Faster than Pacman himself from level 5 on */

    /*! \brief How long a power pellet frightens for; **zero from level 17 on and off
     *         for good from 19**, where a power pellet becomes 50 points and nothing
     *         else. */
    uint32_t frightened_duration_ms;

    /*! \brief How many times the ghosts flash before the window closes; zero when there
     *         is no window. */
    uint8_t frightened_flash_count;

    /*! \brief The scatter/chase plan, **alternating and starting with scatter**: entry 0
     *         is a scatter, entry 1 a chase, and so on. When the entries run out the
     *         ghosts chase for the rest of the level. */
    uint8_t phase_count;
    const uint32_t* phase_durations_ms;
} difficulty_t;

/* ==========================================================================
 * difficulty - public API
 * ========================================================================= */

/*! \brief Look up what a level plays like.
 *
 * Levels above #DIFFICULTY_FINAL_LEVEL are not an error: the arcade's table stops
 * changing at 21 and so does this one, so asking for level 40 gives level 21's row. That
 * keeps the lookup total, which matters because it is called from the middle of a tick.
 *
 * \param[in]       in_level: `1` or above; below that is a programming error
 * \param[out]      out_difficulty: filled in, must not be `NULL`
 */
void difficulty_get(uint8_t in_level, difficulty_t* out_difficulty);

/*! \brief Whether clearing this level ends the run as a win (FR-027). */
bool difficulty_is_final_level(uint8_t in_level);

#endif /* DIFFICULTY_H */
