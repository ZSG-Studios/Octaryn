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
};
}
