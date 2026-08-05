/*
 * ai_weights.c
 *
 * GENERATED — do not edit. Written by Training/export_c.py from ../../../../../../tmp/claude-1000/-home-max-Documents-Projekte-MicroPacControllerMan/5e2e3598-f562-43a3-95a5-70c97d2bc371/scratchpad/dev-winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network b4f18357dc34867a: 27 nodes (0 hidden), 21 connections,
 * 23 inputs, 4 outputs. Trained at stage 2, generation 20,
 * fitness 1932.5.
 */

#include "ai_weights.h"

static const uint16_t g_output_nodes[4] = {
    23U,
    24U,
    25U,
    26U,
};

static const float g_biases[27] = {
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    0x0.0p+0f,
    -0x1.fef99a0000000p-2f,
    0x1.cff07c0000000p+0f,
    -0x1.488ae20000000p-2f,
    -0x1.a6e0c60000000p+0f,
};

static const uint16_t g_connection_offsets[28] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 2U, 8U, 18U, 21U,
};

static const uint16_t g_connection_sources[21] = {
    2U, 19U, 2U, 5U, 6U, 7U, 9U, 19U, 2U, 6U, 7U, 9U, 10U, 11U, 12U, 13U, 18U, 20U, 1U, 13U, 22U,
};

static const float g_connection_weights[21] = {
    -0x1.cbe0200000000p-1f, -0x1.a7e5140000000p-2f, 0x1.6aa1460000000p-1f,  0x1.7b0bae0000000p+1f,
    -0x1.91fc9c0000000p+0f, -0x1.e259c80000000p+1f, 0x1.2103120000000p+1f,  -0x1.6f59360000000p+0f,
    0x1.3f151a0000000p-1f,  0x1.696fcc0000000p+1f,  -0x1.3c14ae0000000p-1f, -0x1.7f88aa0000000p-4f,
    0x1.d0ee380000000p-6f,  -0x1.2f6e3e0000000p+1f, 0x1.e685ec0000000p-5f,  0x1.c0a8d80000000p-1f,
    0x1.65d6c80000000p-4f,  -0x1.c655a00000000p-5f, 0x1.a67d060000000p+0f,  0x1.782c220000000p+0f,
    -0x1.d277120000000p-3f,
};

const neural_net_t g_ai_weights_network = {
    .input_count = 23U,
    .node_count = 27U,
    .output_count = 4U,
    .output_nodes = g_output_nodes,
    .biases = g_biases,
    .connection_offsets = g_connection_offsets,
    .connection_sources = g_connection_sources,
    .connection_weights = g_connection_weights,
};
