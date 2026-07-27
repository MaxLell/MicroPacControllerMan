#include "ott_scenarios.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "custom_assert.h"
#include "ott_display.h"
#include "ott_touchdot.h"
#include "ott_touchpad.h"
#include "ott_user_button.h"

/* ==========================================================================
 * ott_scenarios - private
 * ========================================================================= */

/* To add a test: write ott_<name>.c/.h with a setup and a run function, add one
 * row here, and add the source to CMakeLists.txt. */
static const ott_scenario_t k_scenarios[] = {
    {"user_button", ott_user_button_setup, ott_user_button_run},
    {"touchpad", ott_touchpad_setup, ott_touchpad_run},
    {"display", ott_display_setup, ott_display_run},
    {"touchdot", ott_touchdot_setup, ott_touchdot_run},
};

/* ==========================================================================
 * ott_scenarios - public
 * ========================================================================= */

size_t ott_scenarios_get_count(void)
{
    return sizeof(k_scenarios) / sizeof(k_scenarios[0]);
}

const ott_scenario_t* ott_scenarios_get(size_t in_index)
{
    ASSERT(in_index < ott_scenarios_get_count());

    return &k_scenarios[in_index];
}

bool ott_scenarios_find(const char* const in_name, size_t* out_index)
{
    ASSERT(in_name != NULL);
    ASSERT(out_index != NULL);

    for (size_t index = 0U; index < ott_scenarios_get_count(); ++index)
    {
        if (strcmp(in_name, k_scenarios[index].name) == 0)
        {
            *out_index = index;

            return true;
        }
    }

    return false;
}
