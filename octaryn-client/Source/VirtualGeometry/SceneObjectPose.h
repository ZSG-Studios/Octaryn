#pragma once
#include <array>
#include <string_view>

namespace octaryn::client::rendering {
struct SceneObjectPose {
  std::string_view source_name;
  // Current rigid body transform multiplied by its inverse source bind transform.
  std::array<float,16> delta{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  bool removed{};
};
}
