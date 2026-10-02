#pragma once
#include "../Rendering/RenderBackend/LocalLight.h"
#include <array>

namespace octaryn::client::rendering {
struct MapSceneEnvironment {
  bool enabled{},sky_enabled{true};
  std::array<float,3> ambient{},directional_color{},directional_direction{0,1,0},background{};
};
}
