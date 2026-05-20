#pragma once

#include "Camera.h"
#include "VisibleSectionTraversal.h"

#include <cstdint>

void audit_near_camera_mesh_columns(
    const world_mesh_gpu_buffers &mesh_buffers,
    const visible_section_draw_list &visible_sections, const camera &camera,
    uint64_t frame_index);
