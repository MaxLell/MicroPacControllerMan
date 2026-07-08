#include "ott_scenarios.h"

#include "ott_blinky.h"

#include <string.h>

/*
 * The OTT test registry. To add a test: create ott_<name>.c/.h with a setup and
 * a run function, then add one row here (and the source to CMakeLists). Nothing
 * else in the OTT core or CLI changes.
 */
static const ott_scenario_t k_scenarios[] = {
    {"blinky", ott_blinky_setup, ott_blinky_run},
};

unsigned ott_scenarios_count(void)
{
    return (unsigned)(sizeof(k_scenarios) / sizeof(k_scenarios[0]));
}

const ott_scenario_t* ott_scenarios_get(unsigned index)
{
    return (index < ott_scenarios_count()) ? &k_scenarios[index] : (const ott_scenario_t*)0;
}

int ott_scenarios_find(const char* name)
{
    for (unsigned i = 0; i < ott_scenarios_count(); i++) {
        if (strcmp(name, k_scenarios[i].name) == 0) {
            return (int)i;
        }
    }
    return -1;
}
