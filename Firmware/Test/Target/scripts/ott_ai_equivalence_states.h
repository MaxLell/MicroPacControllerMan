/*
 * ott_ai_equivalence_states.h
 *
 * The shape of the recorded state set VT-INT-024 replays. The data itself is generated —
 * `ott_ai_equivalence_states.c`, written by `Training/record_states.c`.
 *
 * Hand-written on purpose, unlike the `.c`: a generator that emits its own interface can change the
 * interface without anyone noticing, and this is the one place where the host's recording and the
 * target's replay have to agree about what a case *is*.
 */

#ifndef OTT_AI_EQUIVALENCE_STATES_H
#define OTT_AI_EQUIVALENCE_STATES_H

#include <stddef.h>
#include <stdint.h>

#include "ai_weights.h"
#include "msg.h"
#include "playfield.h"

/*! \brief One recorded situation, and what the host build decided about it.
 *
 * The maze travels with the state because the observation asks the playfield about walls and about
 * how much of the level is left. The **pellets do not**: the state's own bitmaps already say where
 * they are, so the target rebuilds the playfield's pellets from them rather than carrying a second
 * copy that could disagree with the first.
 */
typedef struct
{
    const char* what;    /*!< Which of the four situations this is, for the report */
    playfield_map_t map; /*!< The maze it was recorded on                          */
    msg_game_state_t state;
    uint8_t expected_direction; /*!< A \ref direction_e — what the host chose      */
} ai_equivalence_case_t;

extern const ai_equivalence_case_t g_ai_equivalence_cases[];
extern const size_t g_ai_equivalence_case_count;

/*! \brief Digest of the weight table the expectations were recorded against.
 *
 * Compared with #AI_WEIGHTS_DIGEST before anything is replayed. Without that, re-exporting weights
 * without re-recording would report a stale expectation as a porting fault — which is the one
 * failure this test must not invent.
 */
extern const char g_ai_equivalence_digest[];

#endif /* OTT_AI_EQUIVALENCE_STATES_H */
