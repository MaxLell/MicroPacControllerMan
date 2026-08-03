/*
 * ott_high_score.h
 *
 * On-target test: the high-score table, written to real flash (VT-INT-015).
 *
 * The unit tests mock `flash_bsp` away, which is right — an erased page, a page from
 * another build and a page a power cut caught halfway are states a real chip would take an
 * afternoon to produce on purpose. What they cannot check is the part that only exists on
 * silicon: that the page the linker reserved is the page the driver erases, that a
 * 128-bit program actually takes, and that what is read back is what was written.
 *
 * Automatic, and it leaves the table as it found it.
 */

#ifndef OTT_HIGH_SCORE_H
#define OTT_HIGH_SCORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_high_score - public API
 * ========================================================================= */

/*! \brief Erase, write, read back, and put the table back the way it was.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when a score written to flash came back intact
 */
bool ott_high_score_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_HIGH_SCORE_H */
