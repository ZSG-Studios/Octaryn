#include "internal.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace app_lighting_ui_internal {

bool slider_float_with_input(const char* label, float* value, float min, float max, const char* format)
{
    constexpr float slider_width = 170.0f;
    constexpr float input_width = 72.0f;

    ImGui::PushID(label);
    ImGui::SetNextItemWidth(slider_width);
    bool changed = ImGui::SliderFloat("##slider", value, min, max, format);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(input_width);
    changed |= ImGui::InputFloat("##input", value, 0.0f, 0.0f, format, ImGuiInputTextFlags_AutoSelectAll);
    *value = std::clamp(*value, min, max);
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopID();

    return changed;
}

} // namespace app_lighting_ui_internal

bool main_imgui_lighting_begin_frame(main_lighting_tuning_t* tuning)
{
    if (!app_lighting_ui_internal::g_initialized || !tuning || !tuning->visible)
    {
        return false;
    }
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    bool changed = false;
    if (ImGui::Begin("Lighting Tuning", &tuning->visible))
    {
        changed |= app_lighting_ui_internal::slider_float_with_input("Ambient", &tuning->ambient_strength, 0.25f, 3.0f, "%.2f");
        changed |= app_lighting_ui_internal::slider_float_with_input("Sun", &tuning->sun_strength, 0.0f, 3.0f, "%.2f");
        changed |= app_lighting_ui_internal::slider_float_with_input("Fallback Sun", &tuning->sun_fallback_strength, 0.0f, 3.0f, "%.2f");
        changed |= app_lighting_ui_internal::slider_float_with_input("Fog Distance", &tuning->fog_distance, 64.0f, 2048.0f, "%.1f");
        changed |= app_lighting_ui_internal::slider_float_with_input("Sky Ambient Floor", &tuning->skylight_floor, 0.05f, 0.6f, "%.2f");
    }
    ImGui::End();
    ImGui::Render();
    return changed;
}
