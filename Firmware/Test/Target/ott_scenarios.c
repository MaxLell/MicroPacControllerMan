#include "ott_scenarios.h"

#include "ott_button.h"
#include "ott_dispdiag.h"
#include "ott_display.h"
#include "ott_lacheck.h"
#include "ott_touchdot.h"
#include "ott_touchpad.h"

#include <string.h>

/*
 * The OTT test registry. To add a test: create ott_<name>.c/.h with a setup and
 * a run function, then add one row here (and the source to CMakeLists). Nothing
 * else in the OTT core or CLI changes.
 *
 * M2 HAL bring-up: `button`, `touchpad` (I2C1), `display` (SPI1) and `touchdot`
 * (combined touchpad + display) are wired up, plus the `lacheck`/`dispdiag`
 * logic-analyzer diagnostics. (The blinky/LED test was retired — SPI1_SCK is PB3.)
 */
static const ott_scenario_t k_scenarios[] = {
    {"button", ott_button_setup, ott_button_run},
    {"touchpad", ott_touchpad_setup, ott_touchpad_run},
    {"display", ott_display_setup, ott_display_run},
    {"touchdot", ott_touchdot_setup, ott_touchdot_run},
    {"dispdiag", ott_dispdiag_setup, ott_dispdiag_run},
    {"lacheck", ott_lacheck_setup, ott_lacheck_run},
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
