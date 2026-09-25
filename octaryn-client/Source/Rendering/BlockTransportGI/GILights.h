#pragma once
#include "../RenderBackend/LocalLight.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace octaryn::client::rendering {
struct GILightNode {
  std::array<float,4> bounds_min_flux{},bounds_max{};
  std::array<std::uint32_t,4> links{}; // Left, right, light index, leaf count.
};
static_assert(sizeof(GILightNode)==48);
// Root is node zero. Positive lights retain their original local-light index.
std::vector<GILightNode> build_gi_light_tree(std::span<const WorldLocalLight>);
}
