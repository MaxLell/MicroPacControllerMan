/*
 * ai_weights.h
 *
 * GENERATED — do not edit. Written by Training/export_c.py from Training/winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network ec66ee57464204c2: 43 nodes (16 hidden), 432 connections,
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
#define AI_WEIGHTS_DIGEST "ec66ee57464204c2"

#endif /* AI_WEIGHTS_H */
