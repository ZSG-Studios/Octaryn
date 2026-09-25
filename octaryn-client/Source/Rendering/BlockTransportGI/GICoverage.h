#pragma once
#include "../RenderBackend/LocalLight.h"
#include <span>
#include <vector>

namespace octaryn::client::rendering {
using GIEmitterCell=std::array<std::int32_t,4>;
constexpr std::size_t gi_max_emitter_cells=65536;
// Sorted unique cells of the currently selected, positive-energy voxel lights.
std::vector<GIEmitterCell> build_gi_emitter_cells(std::span<const WorldLocalLight>);
}
