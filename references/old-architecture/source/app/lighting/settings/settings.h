#pragma once

#include <SDL3/SDL.h>

#include "app/lighting/ui/imgui.h"

bool main_lighting_settings_load(main_lighting_tuning_t* tuning);
void main_lighting_settings_save(const main_lighting_tuning_t* tuning);
main_lighting_tuning_t main_lighting_settings_default(void);
