#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
struct LightingSettings {
  unsigned reflection_quality{2},shadow_quality{2};
  float sun_angular_radius{.00465f};
  float shadow_history_weight{.85f};
  unsigned shadow_resolution{1024};
  unsigned debug_view{};
  // Distances: zero disables the traced
  // pass (raster shadow fallback / sky-only reflections); 1024 reaches the edge
  // of the loaded chunks at the maximum render distance.
  float shadow_distance{1024}, reflection_distance{1024};
  bool raster_shadows{true};
};
}
