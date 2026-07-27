/*
 * ott_scenarios.h
 *
 * Registry of the available on-target tests. Each test is a self-contained module
 * providing a setup and a run step, and is published by adding one row to the
 * table in ott_scenarios.c — neither the OTT core nor the console changes.
 */

#ifndef OTT_SCENARIOS_H
#define OTT_SCENARIOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ott.h"

/* ==========================================================================
 * ott_scenarios - public types
 * ========================================================================= */

/*! \brief Setup step: validate the console arguments and pack them into the
 *         parameter blob that survives the reset. Runs before the reset.
 *
 * \param[in]       in_argument_count: number of console arguments
 * \param[in]       in_arguments: console arguments, `in_arguments[0]` is the test name
 * \param[out]      out_parameter: buffer of #OTT_PARAMETER_MAX_SIZE bytes
 * \param[out]      out_parameter_size: number of bytes written to `out_parameter`
 * \return          `true` when the arguments were accepted
 */
typedef bool (*ott_setup_fn)(int in_argument_count, char* in_arguments[], uint8_t* out_parameter,
                             uint32_t* out_parameter_size);

/*! \brief Run step: perform the test and judge the outcome. Runs after the reset.
 *
 * \param[in]       in_parameter: blob produced by the setup step
 * \param[in]       in_parameter_size: number of valid bytes in `in_parameter`
 * \param[out]      out_reason: receives a short failure reason when returning `false`
 * \param[in]       in_reason_size: size of `out_reason` in bytes
 * \return          `true` when the test passed
 */
typedef bool (*ott_run_fn)(const uint8_t* in_parameter, uint32_t in_parameter_size,
                           char* out_reason, size_t in_reason_size);

typedef struct
{
    const char* name;                           /*!< Name typed on the console                */
    ott_setup_fn setup_fn;                      /*!< May be `NULL` for a test without arguments */
    ott_run_fn run_fn;                          /*!< Never `NULL`                             */
} ott_scenario_t;

/* ==========================================================================
 * ott_scenarios - public API
 * ========================================================================= */

/*! \brief Return the number of registered scenarios. */
size_t ott_scenarios_get_count(void);

/*! \brief Return a registered scenario by index.
 *
 * \param[in]       in_index: index below #ott_scenarios_get_count
 * \return          The scenario, never `NULL`
 */
const ott_scenario_t* ott_scenarios_get(size_t in_index);

/*! \brief Look a scenario up by its console name.
 *
 * \param[in]       in_name: name to look for, must not be `NULL`
 * \param[out]      out_index: receives the index when found, must not be `NULL`
 * \return          `true` when a scenario of that name is registered
 */
bool ott_scenarios_find(const char* const in_name, size_t* out_index);

#endif /* OTT_SCENARIOS_H */
