/*
 * ott_ai_frame_cost.h — what a frame costs with the agent playing, unattended.
 */

#ifndef OTT_AI_FRAME_COST_H
#define OTT_AI_FRAME_COST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*! \brief Measure the frame budget with inference in the loop and judge it (NFR-006).
 *
 * The same measurement `pacman_ai` takes, without the operator. That test ends by asking somebody
 * at the board to confirm what the agent looks like, so the number it prints has never been part of
 * an unattended run — and the number is the one thing about it a machine can judge on its own.
 *
 * It exists because the shipped weight table changed shape: a NEAT network of 19 connections became
 * a dense 23-16-4 one of 432, so a decision costs about twenty times the multiply-accumulates it
 * used to. Whether that still fits in a frame is a question about this board, not about the host.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: why it failed, when it does
 * \param[in]       in_reason_size: size of that buffer
 * \return          `true` when the session held its frame rate
 */
bool ott_ai_frame_cost_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_AI_FRAME_COST_H */
