#pragma once

#include "imgui.h"

namespace app_lighting_ui_internal {

extern bool g_initialized;

bool slider_float_with_input(const char* label, float* value, float min, float max, const char* format);

} // namespace app_lighting_ui_internal
