/*
 * ai_weights.c
 *
 * GENERATED — do not edit. Written by Training/export_c.py from Training/winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network a082e6ea61e8f6fa: 35 nodes (8 hidden), 19 connections,
 * 23 inputs, 4 outputs. Trained at stage 3, generation 276,
 * fitness 4980.0.
 */

#include "ai_weights.h"

static const uint16_t g_output_nodes[4] = {
    32U,
    33U,
    34U,
    23U,
};

static const float g_biases[35] = {
    0x0p+0f,       0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,
    0x0p+0f,       0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,
    0x0p+0f,       0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,
    0x0p+0f,       0x0p+0f,        -0x1.5c326ap-1f, -0x1.249b42p-1f, -0x1.22147cp+1f, 0x1.765316p+0f, 0x1.625064p+1f,
    0x1.7b412p-2f, 0x1.5fd06cp-1f, -0x1.74cb2p+1f,  -0x1.02a6d4p+0f, 0x1.43323ep-1f,  0x1.76f94p-1f,  0x1.ae0f38p-2f,
};

static const uint16_t g_connection_offsets[36] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,  0U,  0U,  0U,  0U,
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 3U, 5U, 6U, 8U, 10U, 11U, 13U, 16U, 19U,
};

static const uint16_t g_connection_sources[19] = {
    2U, 11U, 24U, 1U, 16U, 21U, 21U, 27U, 20U, 26U, 27U, 30U, 31U, 11U, 25U, 30U, 3U, 28U, 29U,
};

static const float g_connection_weights[19] = {
    -0x1.4689c4p+0f, 0x1.194a5p-1f,  0x1.f263f4p+0f, -0x1.c61018p+1f, 0x1.412902p-1f, 0x1.3e1f18p+0f, 0x1.676926p+3f,
    -0x1.4a2f1cp-1f, 0x1.b4c412p+0f, 0x1.418b88p+0f, 0x1.b958dap+0f,  0x1.d87642p+1f, 0x1.74d9f4p+1f, 0x1.7b80fp+3f,
    -0x1.49c9fcp-3f, 0x1.05f408p+1f, 0x1.9fa346p+2f, 0x1.9731b6p-2f,  0x1.bdcc96p+1f,
};

const neural_net_t g_ai_weights_network = {
    .input_count = 23U,
    .node_count = 35U,
    .output_count = 4U,
    .output_nodes = g_output_nodes,
    .biases = g_biases,
    .connection_offsets = g_connection_offsets,
    .connection_sources = g_connection_sources,
    .connection_weights = g_connection_weights,
};
