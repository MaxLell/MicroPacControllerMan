/*
 * high_score.h
 *
 * The three best scores anyone has managed, kept across a power cycle (FR-009,
 * [10 §10.11](../../../Docu/PrePlanning/10-Pacman-Game-Design.md)).
 *
 * Three rather than one because that is what the menu shows: a single number tells a player
 * whether they were the best, a table tells them how close they came, and the arcade
 * cabinets they are copied from all showed a list.
 *
 * The whole table is held in RAM and written to `Bsp/flash_bsp` only when it changes,
 * which is at most once per run. That is not an optimisation: an erase-and-program on this
 * part stalls the CPU for milliseconds, so writing one on any kind of schedule would show
 * up as a stutter.
 *
 * **Every stored byte is suspect.** A page can be erased, half-programmed by a power cut
 * mid-write, or left over from a different build of this firmware, and all three read back
 * as numbers. A magic word says "this is a high-score table", a version says "of this
 * shape", and a CRC-32 says "and it is intact"; anything that fails starts from an empty
 * table rather than showing a score nobody scored.
 */

#ifndef HIGH_SCORE_H
#define HIGH_SCORE_H

#include <stdbool.h>
#include <stdint.h>

/* ==========================================================================
 * high_score - public types
 * ========================================================================= */

/*! \brief How many scores the table keeps — the three the menu shows. */
#define HIGH_SCORE_COUNT (3U)

/* ==========================================================================
 * high_score - public API
 * ========================================================================= */

/*! \brief Load the table from storage, or start an empty one if there is nothing valid
 *         there.
 *
 * Call once at start-up, after `flash_bsp_init`. It cannot fail from the caller's point of
 * view: an unreadable table and a table nobody has written yet are the same situation, and
 * the answer to both is three zeroes.
 */
void high_score_init(void);

/*! \brief Offer a finished run's score to the table.
 *
 * Stores it only if it beats one of the three, and writes storage only if it changed
 * something. A score equal to one already in the table does not displace it — first to get
 * there keeps the place, which is how the cabinets behaved.
 *
 * A score of zero is never stored: it beats nothing, and a table of zeroes is how "empty"
 * is expressed.
 *
 * \param[in]       in_score: the score to offer
 * \return          `true` when the table changed, so a caller can say so
 */
bool high_score_offer(uint32_t in_score);

/*! \brief One of the scores, best first.
 *
 * \param[in]       in_index: `0`..#HIGH_SCORE_COUNT `- 1`, `0` being the best
 * \return          The score, `0` for a place nobody has taken yet
 */
uint32_t high_score_get(uint8_t in_index);

/*! \brief The best score, or `0` if there is none — what a single-number display shows. */
uint32_t high_score_get_best(void);

/*! \brief Whether a score would get into the table, without offering it.
 *
 * Lets a score screen say "a new high score" before the table has been changed, which is
 * the only way to say it about the run that just ended.
 *
 * \param[in]       in_score: the score to test
 */
bool high_score_would_qualify(uint32_t in_score);

/*! \brief Forget every score, in RAM and in storage.
 *
 * What the `highscore reset` console command does. Erases the page rather than storing
 * three zeroes, so afterwards the storage is in the state it left the factory in — the
 * same state the very first boot sees, which means the empty case gets exercised on demand
 * rather than only once ever.
 *
 * \return          `true` when storage was erased as well as RAM
 */
bool high_score_reset(void);

#endif /* HIGH_SCORE_H */
