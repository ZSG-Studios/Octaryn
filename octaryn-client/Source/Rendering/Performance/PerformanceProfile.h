#pragma once
#include "../RenderBackend/WorldRenderer.h"
#include <cstdlib>
#include <array>
#include <stdexcept>
#include <string_view>

namespace octaryn::client::rendering {
enum class PerformanceProfile {Custom,HQ200};
struct FeatureBudget {const char* name;float milliseconds;};
inline constexpr std::array<FeatureBudget,12> Hq200GpuBudgets{{
  {"visibility",.35f},{"opaque",.65f},{"lighting",.30f},{"shadows",.45f},
  {"reflections",.90f},{"transparency",.35f},{"atmosphere",.25f},{"dynamic",.25f},
  {"reconstruction",.60f},{"ui",.10f},{"streaming",.20f},{"reserve",.60f}}};
inline constexpr std::array<FeatureBudget,7> Hq200CpuBudgets{{
  {"input_ui_audio",.40f},{"transport",.35f},{"render_encoding",1.10f},
  {"streaming",.50f},{"profiling",.15f},{"presentation",.25f},{"reserve",.25f}}};
inline PerformanceProfile requested_performance_profile() {
  const auto* value=std::getenv("OCTARYN_CLIENT_PERFORMANCE_PROFILE");
  if(!value || !*value || std::string_view(value)=="custom")return PerformanceProfile::Custom;
  if(std::string_view(value)=="HQ200")return PerformanceProfile::HQ200;
  throw std::runtime_error("OCTARYN_CLIENT_PERFORMANCE_PROFILE requires custom or HQ200");
}
inline WorldSceneSettings apply_performance_profile(PerformanceProfile profile,WorldSceneSettings settings) {
  if(profile!=PerformanceProfile::HQ200)return settings;
  settings.ray_tracing=true;settings.pbr=true;
  settings.upscaler_mode=6;settings.fsr_render_scale=2.f/3.f;
  settings.fsr_dynamic_resolution=true;settings.fsr_min_scale=.5f;settings.fsr_max_scale=.75f;
  settings.fsr_target_fps=200;settings.fsr_gpu_budget_ms=4.4f;
  return settings;
}
}
