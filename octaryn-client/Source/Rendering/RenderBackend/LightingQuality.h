#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
enum class LightingQuality : std::uint32_t { Low, Medium, High, Ultra };
struct LightingSettings {
  LightingQuality quality{LightingQuality::High};
  float sun_angular_radius{.00465f};
  float shadow_history_weight{.9f};
  unsigned shadow_resolution{1024};
  unsigned debug_view{};
  // Block radii; zero disables the volume. Distances: zero disables the traced
  // pass (raster shadow fallback / sky-only reflections); 1024 reaches the edge
  // of the loaded chunks at the maximum render distance.
  unsigned ddgi_voxel_radius{6}, ddgi_coarse_radius{128};
  float shadow_distance{1024}, reflection_distance{1024};
};
}
