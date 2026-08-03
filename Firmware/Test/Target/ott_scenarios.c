#include "ott_scenarios.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "custom_assert.h"
#include "ott_animation.h"
#include "ott_display_id.h"
#include "ott_display_test.h"
#include "ott_high_score.h"
#include "ott_joystick.h"
#include "ott_joystick_dot.h"
#include "ott_pacman.h"
#include "ott_user_button.h"

/* ==========================================================================
 * ott_scenarios - private
 * ========================================================================= */

/* To add a test: write ott_<name>.c/.h with a setup and a run function, add one
 * row here, and add the source to CMakeLists.txt. */
static const ott_scenario_t g_scenarios[] = {
    {"animation", NULL, ott_animation_run},
    {"display_id", NULL, ott_display_id_run},
    {"display_test", NULL, ott_display_test_run},
    {"high_score", NULL, ott_high_score_run},
    {"joystick", NULL, ott_joystick_run},
    {"joystick_dot", NULL, ott_joystick_dot_run},
    {"pacman", NULL, ott_pacman_run},
    {"user_button", NULL, ott_user_button_run},
};

#define OTT_SCENARIO_COUNT (sizeof(g_scenarios) / sizeof(g_scenarios[0]))

/* ==========================================================================
 * ott_scenarios - public
 * ========================================================================= */

size_t ott_scenarios_get_count(void)
{
    return OTT_SCENARIO_COUNT;
}

const ott_scenario_t* ott_scenarios_get(size_t in_index)
{
    ASSERT(in_index < OTT_SCENARIO_COUNT);

    return &g_scenarios[in_index];
}

bool ott_scenarios_find(const char* const in_name, size_t* out_index)
{
    bool is_found = false;

    ASSERT(in_name != NULL);
    ASSERT(out_index != NULL);

    for (size_t index = 0U; index < OTT_SCENARIO_COUNT; ++index)
    {
        if (strcmp(in_name, g_scenarios[index].name) == 0)
        {
            *out_index = index;
            is_found = true;

            break;
        }
    }

    return is_found;
}
