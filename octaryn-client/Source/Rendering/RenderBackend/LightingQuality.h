#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
enum class LightingQuality : std::uint32_t { Low, Medium, High, Ultra };
// Per-volume GPU work allowance per 1/60 second; spatial/ray quality is stable.
inline double ddgi_quality_milliseconds(LightingQuality quality) {
  switch(quality) {
    case LightingQuality::Low:return .12;
    case LightingQuality::Medium:return .22;
    case LightingQuality::Ultra:return .50;
    default:return .35;
  }
}
struct LightingSettings {
  LightingQuality quality{LightingQuality::High};
  float sun_angular_radius{.00465f};
  float shadow_history_weight{.85f};
  unsigned shadow_resolution{1024};
  unsigned debug_view{};
  // Block radii; zero disables the volume. Distances: zero disables the traced
  // pass (raster shadow fallback / sky-only reflections); 1024 reaches the edge
  // of the loaded chunks at the maximum render distance.
  unsigned ddgi_voxel_radius{6}, ddgi_coarse_radius{128};
  float shadow_distance{1024}, reflection_distance{1024};
  bool raster_shadows{true};
};
}
