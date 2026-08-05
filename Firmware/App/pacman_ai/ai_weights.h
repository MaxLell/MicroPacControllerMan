/*
 * ai_weights.h
 *
 * GENERATED — do not edit. Written by Training/export_c.py from ../../../../../../tmp/claude-1000/-home-max-Documents-Projekte-MicroPacControllerMan/5e2e3598-f562-43a3-95a5-70c97d2bc371/scratchpad/dev-winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network b4f18357dc34867a: 27 nodes (0 hidden), 21 connections,
 * 23 inputs, 4 outputs. Trained at stage see the source file, generation -,
 * fitness -.
 */

#ifndef AI_WEIGHTS_H
#define AI_WEIGHTS_H

#include <stddef.h>

#include "neural_net.h"

/*! \brief The trained network — 23 features in, one score per relative action out. */
extern const neural_net_t g_ai_weights_network;

/*! \brief Which table this is, so a board can be tied back to the run that trained it.
 *         The same figure `Training/evaluate.py` prints. */
#define AI_WEIGHTS_DIGEST "b4f18357dc34867a"

#endif /* AI_WEIGHTS_H */
