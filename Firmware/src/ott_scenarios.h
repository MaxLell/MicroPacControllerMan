#ifndef OTT_SCENARIOS_H
#define OTT_SCENARIOS_H

#include <stdint.h>

/*
 * OTT scenario registry. Each on-target test is a self-contained module that
 * provides a setup step and a run step; it is made available by adding one row
 * to the table in ott_scenarios.c (see the reference OTT design, doc 09). No
 * change to the OTT core or CLI is needed to add a test.
 */

/* setup: validate/parse the CLI args into the retained parameter blob.
 * Returns 1 on success, 0 on failure. Runs on the host side, before the reset. */
typedef int (*ott_setup_fn)(int argc, char* argv[], uint8_t* out_data, uint32_t* out_data_size);

/* run: perform the test and assert the outcome. Returns 1 = PASS, 0 = FAIL
 * (writing a short reason). Runs after the reset, from retained parameters. */
typedef int (*ott_run_fn)(const uint8_t* data, uint32_t data_size, char* reason, unsigned reason_size);

typedef struct {
    const char*  name;
    ott_setup_fn setup; /* may be NULL for tests without parameters */
    ott_run_fn   run;
} ott_scenario_t;

unsigned              ott_scenarios_count(void);
const ott_scenario_t* ott_scenarios_get(unsigned index);
int                   ott_scenarios_find(const char* name); /* index, or -1 */

#endif /* OTT_SCENARIOS_H */
