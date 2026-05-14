#pragma once

#include "Camera.h"
#include "WorldMeshUpload.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct visible_section_draw_list {
  std::vector<size_t> opaque_indices;
  std::vector<size_t> sprite_indices;
  uint32_t total_sections = 0u;
  uint32_t visited_sections = 0u;
  uint32_t frustum_rejected = 0u;
  uint32_t fallback = 0u;
};

visible_section_draw_list build_visible_section_draw_list(
    const world_mesh_gpu_buffers &mesh_buffers, const camera &camera);
