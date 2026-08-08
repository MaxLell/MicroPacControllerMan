/*
 * ai_weights.c
 *
 * GENERATED — do not edit. Written by Training/export_c.py from Training/winner.json.
 * Regenerate with:  python3 Training/export_c.py
 *
 * The network ec66ee57464204c2: 43 nodes (16 hidden), 432 connections,
 * 23 inputs, 4 outputs. Trained at stage 3, generation 2125,
 * fitness 3606.7.
 */

#include "ai_weights.h"

static const uint16_t g_output_nodes[4] = {
    39U,
    40U,
    41U,
    42U,
};

static const float g_biases[43] = {
    0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,
    0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         0x0p+0f,         -0x1.23c332p-1f,
    -0x1.c5c8d2p-1f, -0x1.590bf6p-3f, 0x1.d8e704p-1f,  -0x1.2661ecp-2f, -0x1.8caa3cp-4f, -0x1.4d3d7cp-3f,
    0x1.73066cp-3f,  -0x1.d8b2eep-2f, 0x1.0be78ap+0f,  -0x1.9b82e4p-1f, -0x1.eab99p-2f,  -0x1.b59af4p-2f,
    0x1.de69b6p-3f,  0x1.ef57dp-2f,   -0x1.03dc38p-2f, 0x1.9e4576p+0f,  0x1.9b5a3ep+0f,  0x1.004b66p+1f,
    0x1.c83534p-2f,
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
    0x1.da5908p-1f,  -0x1.601acp-4f,  -0x1.9590fcp+0f, 0x1.38a834p-2f,  -0x1.7ccc5cp+1f, -0x1.ee0522p-1f,
    -0x1.1d2764p-3f, 0x1.56143cp+2f,  0x1.4512bap+0f,  0x1.1deb72p+0f,  -0x1.084ecp+2f,  0x1.d31e82p-1f,
    0x1.00e338p+1f,  -0x1.373044p-1f, -0x1.2c31bep+0f, 0x1.ce373p+1f,   0x1.4bf30ap-2f,  -0x1.914786p+0f,
    -0x1.058448p+1f, 0x1.fbf958p-1f,  -0x1.3f772p+1f,  -0x1.900326p-1f, 0x1.fc4106p-2f,  0x1.2a5f92p+2f,
    -0x1.235398p+0f, -0x1.073b26p-2f, -0x1.a64a66p+0f, 0x1.fd55c6p-2f,  -0x1.d8392p+0f,  0x1.993864p+0f,
    -0x1.53b1dap+1f, 0x1.2c6338p+1f,  -0x1.ba5f76p+0f, -0x1.d1638ep+0f, 0x1.e3a1d8p-1f,  -0x1.1c61fap-2f,
    0x1.34b95cp+0f,  -0x1.ec807p+0f,  0x1.ff07d6p-5f,  0x1.39aef8p-1f,  0x1.a8945cp+0f,  0x1.5e3dd2p+0f,
    -0x1.668eaap+1f, -0x1.2f639ep+0f, 0x1.d7af38p-1f,  0x1.3effbep-1f,  -0x1.5cda5ap+1f, -0x1.2e437ap+1f,
    0x1.308466p-2f,  0x1.617b5cp+0f,  -0x1.5799b2p-3f, -0x1.07603p+0f,  0x1.f2d2dp+0f,   0x1.80ad18p+0f,
    -0x1.544a2ep+0f, -0x1.0b10acp-2f, -0x1.1de9ccp-2f, -0x1.5e609ap-5f, 0x1.2ff7eap+0f,  -0x1.d787dp+0f,
    0x1.20b212p-1f,  0x1.5e7522p-1f,  -0x1.89a6dap-2f, 0x1.8734e6p-1f,  0x1.bf0ad6p+0f,  -0x1.2fd652p+1f,
    0x1.1a6cbep+1f,  0x1.7c6296p-1f,  -0x1.526caep+1f, -0x1.b5f0a2p+0f, -0x1.49638ap-1f, -0x1.3c42ep-1f,
    -0x1.c5b2acp-4f, -0x1.1e0a88p+0f, 0x1.994246p+0f,  -0x1.f95772p+1f, -0x1.489ed4p+1f, 0x1.81826p-1f,
    -0x1.df446ap-1f, 0x1.a5eb6ep+1f,  -0x1.222cfep-2f, 0x1.aaa9f4p+0f,  0x1.681144p-1f,  -0x1.6d0e4ep+1f,
    0x1.f45676p+0f,  0x1.be685ap+0f,  -0x1.9e4e36p+0f, -0x1.3b8eaap+1f, -0x1.b77696p-4f, 0x1.64c602p-2f,
    0x1.aa4628p-1f,  -0x1.1e0596p+0f, -0x1.73735ap+0f, 0x1.1d24e4p-2f,  0x1.848b54p-2f,  -0x1.47d1aap+0f,
    -0x1.4a2608p-2f, 0x1.415648p+2f,  0x1.1a919ap+1f,  -0x1.91a386p-1f, -0x1.566858p+1f, 0x1.86bfbap+1f,
    0x1.064244p+1f,  0x1.0f4a06p+1f,  0x1.829ecep+0f,  -0x1.d130e2p-1f, -0x1.ba37eep-1f, -0x1.4e97bap+0f,
    -0x1.28f2dcp+0f, -0x1.81c91cp-6f, 0x1.f7f52ap-4f,  0x1.039e4cp-1f,  -0x1.6215e4p+2f, -0x1.6713b4p-1f,
    -0x1.439fc6p+0f, 0x1.0f2efcp-1f,  -0x1.63a2a4p+0f, 0x1.36221cp-4f,  -0x1.3ccd34p+2f, 0x1.0f7eb8p-1f,
    0x1.f258e6p+1f,  -0x1.46686cp-1f, -0x1.113f6ap+0f, -0x1.8b83cap+1f, -0x1.d2c6d8p+0f, -0x1.4adb7ap-2f,
    -0x1.b125fep+0f, 0x1.8ddaa6p+0f,  0x1.3f64a2p-3f,  0x1.d11108p+0f,  0x1.4b6392p-1f,  -0x1.15ff68p-6f,
    -0x1.eed962p-1f, -0x1.2dbf1ep+0f, 0x1.402d6ap-2f,  -0x1.27e356p+2f, 0x1.12e0c6p+1f,  -0x1.00d7ep-1f,
    0x1.38dfdp-4f,   0x1.ad77ecp-4f,  0x1.750e7p-1f,   -0x1.597084p+0f, -0x1.1bcc8ap-1f, -0x1.9f2644p-1f,
    0x1.57340ap+1f,  -0x1.917d38p+0f, 0x1.c3652ap-2f,  0x1.8479f2p+1f,  0x1.27d686p-1f,  -0x1.730e3p+0f,
    -0x1.8fd854p+1f, 0x1.711a7p-1f,   0x1.6404bep+0f,  0x1.328074p+1f,  -0x1.f7d9a4p-2f, -0x1.5ff3d4p+1f,
    0x1.27ab84p-2f,  0x1.2ecdd6p+1f,  0x1.d52824p-2f,  -0x1.f0c9cap-5f, 0x1.e662c6p-1f,  0x1.1dd9f6p+1f,
    -0x1.35cc42p+1f, 0x1.3c98b8p+0f,  0x1.83246ap-1f,  -0x1.7f558ep+0f, -0x1.d2cebep-1f, 0x1.1133ap+0f,
    -0x1.2cf452p+0f, -0x1.50b5d8p+1f, 0x1.0c177ap-2f,  0x1.0bc8eap+1f,  0x1.e2377p-2f,   -0x1.928c5p+1f,
    -0x1.5f40aep-2f, -0x1.1c1704p+0f, 0x1.756166p-1f,  -0x1.23df7p+0f,  -0x1.14e242p+0f, -0x1.17c4f6p-1f,
    -0x1.33b2d6p+1f, -0x1.8c08eep+1f, 0x1.4cc614p+0f,  0x1.1fb61ap+0f,  -0x1.cc63d8p-2f, 0x1.1670f8p+0f,
    0x1.bb4aacp+1f,  -0x1.199446p-8f, -0x1.115daap+1f, -0x1.8f0444p-1f, -0x1.9ede2cp-2f, 0x1.704608p+1f,
    0x1.0f83cap-2f,  0x1.5939b2p+0f,  0x1.0705e6p+1f,  -0x1.c6b4d2p+1f, -0x1.70b3bp+0f,  -0x1.da1306p-5f,
    -0x1.ed8a84p-1f, -0x1.263c5p-5f,  0x1.288418p+0f,  0x1.b3c61cp+0f,  -0x1.3f3ee2p+1f, -0x1.509d2cp+0f,
    0x1.7cd0d2p-2f,  0x1.bfa4c6p-2f,  0x1.7dff92p+0f,  -0x1.133b32p+1f, 0x1.e2e9fep+0f,  0x1.f4ffbp+1f,
    0x1.e751d6p-1f,  0x1.b79c92p+1f,  -0x1.e1acbep-1f, 0x1.05429ep+0f,  0x1.d51056p-1f,  -0x1.07348p-2f,
    0x1.589eb8p-1f,  0x1.2de93cp-2f,  -0x1.a6b85ep+0f, 0x1.5ff8ecp-1f,  -0x1.2c6334p+2f, 0x1.803978p+1f,
    0x1.d65f74p-1f,  0x1.e0b082p+0f,  0x1.16cd3ep+1f,  0x1.316752p+0f,  -0x1.b0d4fp+0f,  -0x1.9a1cdap+0f,
    0x1.0e43dcp+1f,  0x1.aeb536p-2f,  0x1.2538c4p+1f,  -0x1.3fdfep-3f,  -0x1.487d6cp+0f, 0x1.42db06p-1f,
    -0x1.504cfcp-1f, 0x1.56c4a6p-4f,  -0x1.3a2e9p+0f,  -0x1.3b3d56p+1f, -0x1.941a2p+1f,  -0x1.676316p+1f,
    -0x1.b35852p+0f, -0x1.765b44p+0f, 0x1.9aa8c4p+0f,  0x1.21ca1cp+1f,  -0x1.8f84cp+0f,  -0x1.51dd5ap+0f,
    0x1.aa241p-3f,   0x1.2a88dep-1f,  -0x1.42236ap-1f, 0x1.ac488ep-1f,  -0x1.e7503cp-3f, 0x1.b76d7p+0f,
    0x1.071196p+0f,  -0x1.717ae8p+1f, -0x1.de69bep+0f, 0x1.7fac92p-3f,  -0x1.c3857ap+1f, -0x1.414b76p-1f,
    -0x1.6e9b28p-1f, 0x1.73bf8p+0f,   -0x1.fed5bcp+0f, -0x1.32c846p-1f, 0x1.bb4512p+0f,  -0x1.f5aed2p-1f,
    -0x1.d81d1cp+0f, 0x1.42a9ecp+1f,  0x1.d91838p-1f,  0x1.0ae574p+0f,  0x1.4297aap+1f,  0x1.0a9668p-3f,
    0x1.174bf4p-1f,  0x1.5e2908p-6f,  -0x1.353366p+1f, 0x1.84d4aap+1f,  0x1.2e329cp+0f,  -0x1.84a024p+2f,
    0x1.09c212p+0f,  -0x1.a3a18cp-4f, 0x1.4d6e32p+1f,  -0x1.56b01cp+1f, -0x1.c0f138p+0f, 0x1.f94bfap-1f,
    -0x1.81f7e2p+0f, 0x1.2d88cp+1f,   -0x1.389494p+0f, -0x1.5d2f1ep-1f, 0x1.9fde92p+1f,  -0x1.acf478p-2f,
    -0x1.04cd34p-1f, -0x1.59743cp+0f, -0x1.4e8178p+0f, -0x1.21a394p+1f, 0x1.5bd1ccp-1f,  -0x1.10e31cp-1f,
    0x1.781e46p-1f,  -0x1.5d157p-3f,  -0x1.4394b2p+1f, 0x1.7850cp-1f,   0x1.d217e8p+0f,  -0x1.79df5ep+0f,
    0x1.02438p+0f,   -0x1.cebf1p+1f,  0x1.2f6cecp+1f,  -0x1.9328ep+0f,  -0x1.7580ccp+1f, -0x1.579aeap+1f,
    -0x1.933a7cp-1f, 0x1.1c00cap+0f,  -0x1.286b76p+0f, 0x1.565b62p-1f,  -0x1.a3ba2ap+0f, 0x1.c8b888p-1f,
    0x1.4ad5aep-1f,  -0x1.2b87ccp-2f, 0x1.40a0b4p+0f,  0x1.547f5ap+1f,  0x1.63e7aep+0f,  0x1.f324d6p+1f,
    0x1.339074p-4f,  -0x1.8d61a6p+1f, 0x1.1fca7ep+1f,  -0x1.f2efc4p+0f, 0x1.91dba8p-1f,  -0x1.7069e8p+0f,
    0x1.7fe91p-1f,   0x1.033f0cp+0f,  -0x1.28457p+1f,  0x1.dd25fap-2f,  -0x1.41ecdcp-1f, 0x1.132026p+1f,
    -0x1.fd6ccap-2f, 0x1.e811d6p+1f,  -0x1.94a71p-1f,  0x1.9d62ecp+1f,  -0x1.8a9878p-2f, 0x1.f59f62p-2f,
    0x1.8d0f86p+0f,  -0x1.76def4p+1f, 0x1.5b27dep+0f,  0x1.32ed36p+0f,  0x1.f2d846p+0f,  -0x1.f1cf6cp+0f,
    0x1.213a6ep-1f,  0x1.355294p-1f,  0x1.16f3cep-1f,  -0x1.37609ep-3f, -0x1.25c656p+1f, 0x1.27276cp+0f,
    -0x1.d5028ep+0f, 0x1.180cbep-1f,  -0x1.202df6p-1f, -0x1.0ba37p+0f,  0x1.b433ecp+1f,  0x1.d7adbep-3f,
    0x1.5946acp-7f,  -0x1.e7bc58p-1f, 0x1.ba431cp-1f,  0x1.557706p-2f,  -0x1.38bf06p+2f, 0x1.af47dap+0f,
    -0x1.8c299p+1f,  0x1.01fb82p-2f,  -0x1.87814ap-2f, 0x1.82c5d4p-4f,  -0x1.87bae2p-1f, 0x1.3350fep+0f,
    0x1.7f9624p+2f,  -0x1.eda816p-2f, 0x1.a10c92p-2f,  0x1.1e090ep+1f,  -0x1.c482ccp-3f, 0x1.130276p-1f,
    0x1.afa806p-5f,  0x1.dc8c24p-1f,  0x1.d30348p+0f,  0x1.e34b36p-2f,  0x1.a51d46p+1f,  -0x1.19254cp+1f,
    -0x1.3782dp+1f,  0x1.57ebaap+2f,  -0x1.54acbap+0f, 0x1.4eb19p+1f,   0x1.b4a83ap+1f,  0x1.d6ab88p+0f,
    -0x1.b59b7cp-2f, -0x1.3cfb8ap+1f, -0x1.74ca66p-2f, -0x1.175324p+2f, 0x1.cbd114p-1f,  -0x1.4fcc9ap-1f,
    0x1.7c3f08p-1f,  -0x1.98f546p+0f, 0x1.d2c982p-5f,  0x1.cab72cp-1f,  0x1.bd9674p-4f,  0x1.6ced9cp-2f,
    -0x1.b11c0ep+1f, -0x1.a85faep-2f, 0x1.72d5c2p-2f,  -0x1.f4a7b6p-1f, 0x1.ed807p-1f,   0x1.5a1592p-3f,
    0x1.0672fep+2f,  0x1.88b332p-7f,  0x1.8f7708p+1f,  -0x1.2848f8p+1f, 0x1.181caep+2f,  0x1.f12eecp+1f,
    0x1.7c718cp+2f,  0x1.5bd9fp-2f,   -0x1.47d756p+1f, -0x1.b85f76p-2f, -0x1.17f9a4p+0f, 0x1.30862p+0f,
    -0x1.bca87ep+1f, 0x1.b3f5dcp+0f,  0x1.49deaep+0f,  -0x1.23334ep-2f, 0x1.086246p+1f,  0x1.6841acp-1f,
    0x1.4c9544p-2f,  -0x1.9c1f9p-2f,  -0x1.99a9bp+0f,  0x1.16a528p+0f,  0x1.17755ap+1f,  -0x1.6cf474p+2f,
    -0x1.4d09dep+0f, 0x1.801938p+1f,  -0x1.756076p+2f, 0x1.b3dc0ep-2f,  -0x1.073c0cp-2f, -0x1.bbe1aep+1f,
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
