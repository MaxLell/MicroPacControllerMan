/*
 * ai_weights.c
 *
 * GENERATED — do not edit. Written by Training/export_c.py from Training/winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network 41cc70f5ce88b97e: 43 nodes (16 hidden), 432 connections,
 * 23 inputs, 4 outputs. Trained at stage 3, generation 4049,
 * fitness 8612.5.
 */

#include "ai_weights.h"

static const uint16_t g_output_nodes[4] = {
    39U,
    40U,
    41U,
    42U,
};

static const float g_biases[43] = {
    0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         0x0p+0f,        0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         -0x1.2ec5cp+0f, 0x1.99f82p+0f,   -0x1.0359c6p+0f, 0x1.0009f6p+1f,  0x1.23515ap-1f,
    -0x1.ec66e6p-4f, -0x1.d7b8cap-3f, 0x1.811036p-1f, 0x1.41feaep+1f,  -0x1.72b136p-2f, -0x1.83761ap-2f, -0x1.f62eep-3f,
    -0x1.4c7e96p-5f, 0x1.51b566p-1f,  0x1.53a8f2p-2f, -0x1.9d19a4p-4f, 0x1.d450d8p+0f,  0x1.fd7834p-2f,  0x1.11066ap+1f,
    -0x1.cc794ep-6f,
};

static const uint16_t g_connection_offsets[44] = {
    0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,
    0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,   23U,  46U,  69U,  92U,  115U, 138U,
    161U, 184U, 207U, 230U, 253U, 276U, 299U, 322U, 345U, 368U, 384U, 400U, 416U, 432U,
};

static const uint16_t g_connection_sources[432] = {
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    0U,  1U,  2U,  3U,  4U,  5U,  6U,  7U,  8U,  9U,  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U,
    23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 34U, 35U, 36U, 37U, 38U, 23U, 24U, 25U, 26U, 27U, 28U, 29U,
    30U, 31U, 32U, 33U, 34U, 35U, 36U, 37U, 38U, 23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 34U, 35U, 36U,
    37U, 38U, 23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 34U, 35U, 36U, 37U, 38U,
};

static const float g_connection_weights[432] = {
    0x1.d0a0b6p+0f,  0x1.556b28p+1f,  0x1.2ad85ap-1f,  0x1.062ac6p+2f,  -0x1.83266p+0f,  -0x1.e7eea2p+1f,
    -0x1.895edcp+0f, 0x1.ccc124p-1f,  0x1.2adfa2p+2f,  0x1.e0561ep-1f,  0x1.900cf2p+0f,  0x1.a962fep-1f,
    0x1.218b4cp+1f,  -0x1.c7775cp-1f, -0x1.ec776ep+1f, -0x1.556594p-2f, -0x1.4ba31ep+0f, -0x1.b0809p-6f,
    0x1.01ec62p+0f,  -0x1.3136d8p-2f, 0x1.690b72p+0f,  0x1.7409bep+1f,  -0x1.fc29a4p+0f, -0x1.ea8e28p+1f,
    0x1.3616bap+1f,  0x1.63fdd2p+1f,  -0x1.2a9024p+1f, -0x1.c83fd4p-5f, 0x1.70e7e8p+1f,  0x1.132488p+1f,
    -0x1.6843b4p+1f, -0x1.0d966ap-2f, -0x1.a619a2p-5f, -0x1.598856p+1f, -0x1.e70dfcp+1f, 0x1.97fc3ep+1f,
    0x1.fe688ep+1f,  -0x1.9ced4cp-1f, 0x1.8c9d18p-1f,  0x1.cb3e54p-3f,  -0x1.d9a61ap+0f, -0x1.09572ep+2f,
    0x1.34af1p+0f,   0x1.0914bp+2f,   -0x1.0677e2p-1f, -0x1.ea1f9p+0f,  -0x1.b378c8p+0f, 0x1.f0dfeap+0f,
    0x1.b3748p+0f,   -0x1.8744a6p+1f, -0x1.5570d4p-1f, -0x1.4ed05ep-2f, 0x1.3cbbacp+2f,  0x1.99435cp-1f,
    -0x1.46d0aep-3f, -0x1.27fc4p+1f,  -0x1.91fff6p+1f, 0x1.ab1c3ep+1f,  -0x1.112a7ep+1f, -0x1.057e9cp-1f,
    0x1.014a4ep+2f,  -0x1.b49dp-1f,   -0x1.64fd4cp-1f, 0x1.fa6fcep-3f,  -0x1.49ddecp-7f, 0x1.b3c9cep+1f,
    -0x1.08769ep+0f, 0x1.9316a2p-2f,  -0x1.f74eb8p+0f, 0x1.2b2116p+0f,  0x1.2413c8p+1f,  -0x1.d638f4p+1f,
    -0x1.ae75f4p-1f, 0x1.3f0922p-4f,  0x1.005758p+1f,  -0x1.5946c6p+1f, -0x1.e2c1e4p+1f, -0x1.d35102p-3f,
    -0x1.e1651ap-4f, -0x1.f4fbfcp+0f, -0x1.9eaba2p+2f, -0x1.c36826p-1f, -0x1.3b63d8p+1f, -0x1.ff8696p+1f,
    0x1.8f6484p+0f,  0x1.4815c6p+2f,  0x1.5a6358p+0f,  0x1.632f08p+1f,  0x1.f5a4acp-2f,  -0x1.f5a1dp+1f,
    -0x1.407544p-1f, -0x1.f0f06cp+1f, -0x1.77eabap+0f, -0x1.a946cap+1f, 0x1.d32baap+1f,  0x1.a6b07ep-1f,
    -0x1.661742p-1f, 0x1.469b46p+0f,  0x1.648b3ap+0f,  -0x1.138a6p+1f,  -0x1.dac134p+0f, 0x1.1de594p-1f,
    0x1.9f7ba2p-1f,  0x1.969366p+1f,  -0x1.3826fp+1f,  -0x1.3d0d6p+1f,  0x1.b113ecp+1f,  0x1.7bb156p-5f,
    0x1.b3c324p-1f,  0x1.961c32p-2f,  -0x1.7a3896p-1f, 0x1.df63ap+0f,   -0x1.1c5bb8p+1f, -0x1.da4d88p-3f,
    -0x1.13268p-2f,  -0x1.1f15cep+0f, -0x1.9b5b68p+0f, 0x1.ce2442p+1f,  -0x1.250386p+2f, 0x1.e8b1bp-1f,
    -0x1.df7da2p+1f, -0x1.3212dap+1f, 0x1.e35dcep+0f,  -0x1.39067p-1f,  0x1.f7c29p-2f,   0x1.35c332p+1f,
    -0x1.a30194p+0f, -0x1.b71e84p+1f, -0x1.fd9398p-1f, 0x1.2dee4cp-2f,  -0x1.268a46p-1f, 0x1.bc454ap+0f,
    -0x1.fef7dcp+0f, -0x1.59d8c6p+2f, 0x1.6b8e1ap+2f,  0x1.7bf74ep+1f,  -0x1.11139cp+0f, 0x1.41207ap+1f,
    -0x1.1b034ap-3f, 0x1.d081dp-1f,   -0x1.a6f62cp+1f, -0x1.b32062p-2f, -0x1.1ec8d2p-2f, 0x1.674d78p+1f,
    0x1.ee2542p+0f,  -0x1.bf434cp-6f, -0x1.d2f8eep-8f, 0x1.1c15bap+2f,  0x1.2353d2p+1f,  0x1.cc4414p+1f,
    -0x1.6eb02ep+0f, -0x1.0aee92p-1f, 0x1.441bc4p+0f,  -0x1.8b195ap-1f, -0x1.92a3cap+1f, -0x1.135c6ep+1f,
    -0x1.18e28cp+1f, -0x1.f34b0ap-4f, -0x1.a108aap+1f, 0x1.4e738cp+0f,  0x1.b3f324p+0f,  -0x1.c1da12p+0f,
    -0x1.814eecp+0f, -0x1.9c23p-2f,   0x1.f38718p+0f,  -0x1.52d1fp+0f,  0x1.604312p+0f,  -0x1.4df434p+0f,
    -0x1.f64ae2p+0f, 0x1.03e62ep+0f,  0x1.7e2056p+0f,  -0x1.12cc8cp+1f, 0x1.19474p+0f,   0x1.20dfa8p+2f,
    -0x1.476a18p+1f, -0x1.769c44p-4f, 0x1.78cc5p-2f,   0x1.263d12p+0f,  -0x1.04e19p-1f,  -0x1.9de05ap-1f,
    -0x1.104d1p+1f,  -0x1.afc3f6p+0f, -0x1.3b77c8p+0f, 0x1.7cb636p-2f,  0x1.2d9486p-2f,  -0x1.7f003ep+1f,
    -0x1.359fb8p+0f, 0x1.4fa814p+0f,  -0x1.df4488p+1f, 0x1.5a367cp+1f,  0x1.c1302p+1f,   0x1.395468p-2f,
    -0x1.94b8dap+1f, -0x1.2b16bcp-5f, 0x1.a23e7cp-1f,  -0x1.0f8e52p-2f, -0x1.1907c8p+0f, 0x1.a921fcp+0f,
    -0x1.43d31cp-3f, 0x1.34f452p+1f,  -0x1.416338p-1f, 0x1.46cc06p+0f,  0x1.5e3112p+0f,  -0x1.ed9ad8p-1f,
    -0x1.69e548p+0f, 0x1.f5b806p+0f,  -0x1.409498p+0f, -0x1.8050dcp-3f, -0x1.587304p-3f, -0x1.000f7p+0f,
    0x1.b4fe38p-2f,  -0x1.3033c2p+1f, -0x1.a9bdccp+0f, 0x1.930c5ep+1f,  0x1.29407cp+1f,  -0x1.3064c4p+1f,
    0x1.afbb6p+0f,   -0x1.81648cp+1f, -0x1.3b84fep+2f, -0x1.950c0cp+0f, -0x1.b4f0e8p-1f, 0x1.99ae04p+0f,
    0x1.e90a3cp+0f,  0x1.c98892p-2f,  -0x1.b31a42p-3f, -0x1.4a401cp-5f, -0x1.a5779p-1f,  -0x1.5a9ee4p+0f,
    -0x1.5f919ap-2f, 0x1.960106p+0f,  -0x1.0a66c4p-3f, 0x1.9f4a9p-2f,   0x1.6ba37ep+1f,  0x1.053d6cp-2f,
    0x1.d30636p+0f,  -0x1.81847cp+1f, -0x1.c01e64p-6f, -0x1.66437ap+1f, -0x1.3a95ep+1f,  -0x1.432442p+1f,
    0x1.b99f64p+0f,  -0x1.bf7606p+0f, 0x1.6e5cc4p-5f,  0x1.753e8cp+2f,  -0x1.a0dbdep+0f, 0x1.a261fep+0f,
    0x1.fe2cb2p+1f,  -0x1.7ebde8p+2f, 0x1.4f5454p+2f,  -0x1.790d8ep+0f, 0x1.86a0eep-1f,  0x1.0c7606p+1f,
    -0x1.2659aep+0f, -0x1.4a87bp+2f,  0x1.82b2e2p-1f,  0x1.160eeep+2f,  0x1.94d252p-4f,  0x1.9755fp+1f,
    -0x1.6820f4p-1f, 0x1.05540ap-2f,  0x1.53afep-2f,   -0x1.93d6ep+0f,  0x1.e5959cp+1f,  0x1.796828p-2f,
    0x1.5076b4p+1f,  -0x1.63f9f8p-5f, 0x1.f74994p-3f,  -0x1.3be98p+0f,  -0x1.274ed4p-1f, -0x1.fb9eeep-1f,
    0x1.ecebecp-4f,  -0x1.3d2914p+0f, 0x1.b425fap+0f,  -0x1.6ff968p+0f, 0x1.7c41eap-1f,  -0x1.b1a066p+0f,
    -0x1.67d6fap+0f, 0x1.39e1b2p+2f,  -0x1.b359fcp+1f, -0x1.3696c4p+1f, -0x1.ec693ap-1f, -0x1.64df18p+0f,
    0x1.00144ep+2f,  0x1.1019c2p-1f,  -0x1.623a36p-7f, -0x1.2e91fep-6f, -0x1.a93e4ap+0f, -0x1.36dbfap+1f,
    0x1.97a3cep-4f,  0x1.e420dp+0f,   0x1.13eb36p+2f,  -0x1.302a18p+0f, -0x1.b27848p+0f, -0x1.1602cp+1f,
    -0x1.b499e2p+1f, 0x1.f437bep-3f,  0x1.e78506p-2f,  0x1.2ac6aep+0f,  -0x1.5cdc5p-3f,  0x1.e0e096p-2f,
    -0x1.d6e08p+1f,  0x1.1bc256p+2f,  0x1.d4d96ap+1f,  -0x1.0aecc2p+2f, -0x1.493646p+1f, -0x1.5b353cp+0f,
    0x1.1f4b6ep-2f,  -0x1.79a202p+2f, 0x1.07cbep+1f,   -0x1.e87efap+1f, -0x1.13bc72p+2f, -0x1.7a2d2ap+1f,
    0x1.42153ep+0f,  0x1.a5006cp+1f,  -0x1.ffa2cep-1f, 0x1.b62dep+2f,   0x1.5dfa7cp-4f,  -0x1.249124p+1f,
    -0x1.4a720ep+0f, 0x1.346dd8p+2f,  -0x1.c47e12p-2f, -0x1.bd879cp-1f, 0x1.bafb58p+0f,  -0x1.2ea9bap-1f,
    -0x1.fd6f16p-2f, 0x1.8dc0fp-2f,   0x1.101e22p-1f,  0x1.7b887cp+0f,  -0x1.b11296p+0f, -0x1.0dd248p+1f,
    -0x1.39d00cp+0f, -0x1.9aba1ap-7f, 0x1.6aef4cp-2f,  0x1.dba62ep+2f,  0x1.c31bcap+1f,  0x1.f8693cp+0f,
    0x1.d1fc1ap-1f,  0x1.85fe3ep+0f,  -0x1.155864p+1f, -0x1.cee364p+1f, 0x1.49cb2p+0f,   -0x1.4d20b4p+0f,
    0x1.5c4a96p-2f,  0x1.99ef62p-4f,  -0x1.b27f18p+1f, 0x1.612568p-2f,  0x1.47c9ap+1f,   -0x1.59fc1cp-1f,
    -0x1.ecc63cp-1f, -0x1.d7711cp-1f, -0x1.4db8bp+1f,  0x1.00a562p+0f,  0x1.4c10a2p-2f,  0x1.af8d42p+0f,
    -0x1.63b9b8p+0f, 0x1.b83826p+1f,  -0x1.8cc198p+1f, 0x1.930468p+1f,  -0x1.860036p+2f, -0x1.866c1cp+1f,
    0x1.6856c6p-2f,  -0x1.27c35ap+1f, -0x1.971b7ep-5f, 0x1.8668c4p-3f,  0x1.85b1fp+2f,   -0x1.0fa434p+0f,
    0x1.95bb3p+2f,   0x1.550af8p+1f,  0x1.06fa84p+2f,  0x1.0b0814p+0f,  -0x1.052406p+2f, 0x1.bf1e64p-2f,
    -0x1.63b266p-2f, -0x1.847d4ep+2f, -0x1.d3a168p+2f, -0x1.f9a89ep+0f, 0x1.4c38b8p+2f,  0x1.7e2134p+1f,
    -0x1.f64804p+1f, -0x1.adc3a2p+1f, 0x1.acd1a4p+0f,  0x1.41129ap+2f,  -0x1.3f291p-1f,  0x1.7167c8p-1f,
    -0x1.22278cp+2f, 0x1.6a5e76p+0f,  0x1.c1fe7ep+1f,  0x1.ca79bep-2f,  0x1.3e983p+0f,   0x1.4b22bcp-2f,
    -0x1.8a6adcp+1f, -0x1.20188ep-2f, -0x1.2fb0b4p+2f, 0x1.1edeeap-2f,  -0x1.0b316p-2f,  0x1.e418f2p+0f,
    -0x1.aa6edap+0f, 0x1.59ab38p+0f,  0x1.43062ap+2f,  0x1.044434p-1f,  0x1.139adp-2f,   -0x1.ea19dp-1f,
    0x1.01499p+2f,   -0x1.60071p+2f,  0x1.9cc712p-1f,  -0x1.b29732p+1f, -0x1.f52bp+1f,   0x1.9783e4p+1f,
    0x1.a9deaep+2f,  -0x1.704464p+2f, 0x1.ce2334p+1f,  0x1.d5c098p+1f,  -0x1.47a352p-4f, -0x1.9ea69cp+1f,
    -0x1.5b634ep+2f, 0x1.1d7e82p+0f,  -0x1.44357ap-3f, -0x1.787caep-1f, -0x1.1d19e4p+0f, 0x1.99eaf6p+1f,
    0x1.050444p+0f,  -0x1.e55b8p-1f,  -0x1.457072p-1f, 0x1.129f7p+1f,   -0x1.a802c8p+2f, 0x1.6c6cdp+2f,
    -0x1.a42884p+0f, -0x1.66bcbep+1f, -0x1.52f6a8p+1f, -0x1.eb30cep-3f, -0x1.4af5ccp-4f, -0x1.11c8e2p+2f,
};

const neural_net_t g_ai_weights_network = {
    .input_count = 23U,
    .node_count = 43U,
    .output_count = 4U,
    .output_nodes = g_output_nodes,
    .biases = g_biases,
    .connection_offsets = g_connection_offsets,
    .connection_sources = g_connection_sources,
    .connection_weights = g_connection_weights,
};
