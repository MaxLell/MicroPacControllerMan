/*
 * ott_ai_equivalence.h
 *
 * On-target test: the ported inference chooses what the host chose (VT-INT-024, FR-039).
 *
 * The maze generator's own discipline applied to the network — compare the port against its
 * original rather than believe it
 * ([DEC-029](../../../../Docu/PrePlanning/11-Decisions-and-As-Built.md)). The two traps
 * [M6 §9](../../../../Docu/Design/M6-Pacman-AI.md) designs out are the ones this would catch: the
 * host promoting float arithmetic to `double`, and a `tanh` out of glibc that does not agree with
 * newlib's to the last bit.
 *
 * Automatic, fast, and it changes nothing: four recorded states, four decisions, four comparisons.
 */

#ifndef OTT_AI_EQUIVALENCE_H
#define OTT_AI_EQUIVALENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_ai_equivalence - public API
 * ========================================================================= */

/*! \brief Replay the recorded states through this build's inference and compare.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the first state that disagreed
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when every recorded state decided the same way here
 */
bool ott_ai_equivalence_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_AI_EQUIVALENCE_H */
