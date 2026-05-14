#include "VisibleSectionTraversal.h"

#include "Packing.h"

#include <cmath>
#include <queue>

namespace {

struct traversal_node {
  size_t section_index = 0u;
  uint32_t entry_face = 6u;
};

bool section_near_camera(const camera &camera,
                         const world_render_section_key &key) {
  constexpr float kMargin = 32.0f;
  const float min_x = static_cast<float>(key.x * kWorldRenderSectionSize) -
                      kMargin;
  const float max_x = static_cast<float>((key.x + 1) * kWorldRenderSectionSize) +
                      kMargin;
  const float min_y = static_cast<float>(key.y * kWorldRenderSectionSize) -
                      kMargin;
  const float max_y = static_cast<float>((key.y + 1) * kWorldRenderSectionSize) +
                      kMargin;
  const float min_z = static_cast<float>(key.z * kWorldRenderSectionSize) -
                      kMargin;
  const float max_z = static_cast<float>((key.z + 1) * kWorldRenderSectionSize) +
                      kMargin;
  return camera.position[0] >= min_x && camera.position[0] <= max_x &&
         camera.position[1] >= min_y && camera.position[1] <= max_y &&
         camera.position[2] >= min_z && camera.position[2] <= max_z;
}

bool section_box_visible(const camera &camera,
                         const world_render_section_key &key) {
  if (camera.projection_mode == CAMERA_PROJECTION_ORTHOGRAPHIC ||
      section_near_camera(camera, key)) {
    return true;
  }
  return camera_is_box_visible(
             &camera, static_cast<float>(key.x * kWorldRenderSectionSize),
             static_cast<float>(key.y * kWorldRenderSectionSize),
             static_cast<float>(key.z * kWorldRenderSectionSize),
             static_cast<float>(kWorldRenderSectionSize),
             static_cast<float>(kWorldRenderSectionSize),
             static_cast<float>(kWorldRenderSectionSize)) != 0;
}

world_render_section_key key_from_chunk(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return {chunk.chunk_x, chunk.chunk_y, chunk.chunk_z};
}

world_render_section_key camera_section_key(
    const camera &camera) {
  return {floor_div_int32(static_cast<int32_t>(std::floor(camera.position[0])),
                          kWorldRenderSectionSize),
          floor_div_int32(static_cast<int32_t>(std::floor(camera.position[1])),
                          kWorldRenderSectionSize),
          floor_div_int32(static_cast<int32_t>(std::floor(camera.position[2])),
                          kWorldRenderSectionSize)};
}

size_t nearest_section_index(const world_mesh_gpu_buffers &mesh_buffers,
                             const world_render_section_key &camera_key) {
  size_t best = mesh_buffers.sections.size();
  uint64_t best_distance = UINT64_MAX;
  for (const auto &entry : mesh_buffers.section_indices) {
    const size_t index = entry.second;
    if (index >= mesh_buffers.sections.size()) {
      continue;
    }
    const world_render_section_key &key = mesh_buffers.sections[index].key;
    const int64_t dx = static_cast<int64_t>(key.x) - camera_key.x;
    const int64_t dy = static_cast<int64_t>(key.y) - camera_key.y;
    const int64_t dz = static_cast<int64_t>(key.z) - camera_key.z;
    const uint64_t distance =
        static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
    if (distance < best_distance) {
      best = index;
      best_distance = distance;
    }
  }
  return best;
}

void append_fallback_draws(const world_mesh_gpu_buffers &mesh_buffers,
                           const camera &camera,
                           visible_section_draw_list &list) {
  list.fallback = 1u;
  for (size_t index = 0u; index < mesh_buffers.chunks.size(); ++index) {
    const auto &chunk = mesh_buffers.chunks[index];
    const world_render_section_key key = key_from_chunk(chunk.record);
    if (!section_box_visible(camera, key)) {
      ++list.frustum_rejected;
      continue;
    }
    if (chunk.record.opaque_face_count != 0u && chunk.opaque_faces != nullptr) {
      list.opaque_indices.push_back(index);
    }
    if (chunk.record.sprite_index_count != 0u &&
        chunk.sprite_vertices != nullptr) {
      list.sprite_indices.push_back(index);
    }
  }
}

} // namespace

visible_section_draw_list build_visible_section_draw_list(
    const world_mesh_gpu_buffers &mesh_buffers,
    const camera &camera) {
  visible_section_draw_list list{};
  list.total_sections = static_cast<uint32_t>(mesh_buffers.sections.size());
  if (mesh_buffers.sections.empty() || mesh_buffers.section_indices.empty()) {
    append_fallback_draws(mesh_buffers, camera, list);
    return list;
  }

  const world_render_section_key camera_key = camera_section_key(camera);
  auto start = mesh_buffers.section_indices.find(camera_key);
  size_t start_index =
      start == mesh_buffers.section_indices.end()
          ? nearest_section_index(mesh_buffers, camera_key)
          : start->second;
  if (start_index >= mesh_buffers.sections.size()) {
    append_fallback_draws(mesh_buffers, camera, list);
    return list;
  }

  std::vector<uint8_t> visited(mesh_buffers.sections.size(), 0u);
  std::queue<traversal_node> pending;
  pending.push({start_index, 6u});
  constexpr int32_t dx[6] = {0, 0, 1, -1, 0, 0};
  constexpr int32_t dy[6] = {0, 0, 0, 0, 1, -1};
  constexpr int32_t dz[6] = {1, -1, 0, 0, 0, 0};

  while (!pending.empty()) {
    const traversal_node node = pending.front();
    pending.pop();
    if (visited[node.section_index] != 0u) {
      continue;
    }
    visited[node.section_index] = 1u;
    ++list.visited_sections;

    const world_render_section_state &section =
        mesh_buffers.sections[node.section_index];
    const bool visible = section_box_visible(camera, section.key);
    if (!visible) {
      ++list.frustum_rejected;
    } else if (auto chunk = mesh_buffers.chunk_indices.find(section.key);
               chunk != mesh_buffers.chunk_indices.end()) {
      const auto &gpu_chunk = mesh_buffers.chunks[chunk->second];
      if (gpu_chunk.record.opaque_face_count != 0u &&
          gpu_chunk.opaque_faces != nullptr) {
        list.opaque_indices.push_back(chunk->second);
      }
      if (gpu_chunk.record.sprite_index_count != 0u &&
          gpu_chunk.sprite_vertices != nullptr) {
        list.sprite_indices.push_back(chunk->second);
      }
    }

    if (!visible && !section_near_camera(camera, section.key)) {
      continue;
    }
    for (uint32_t side = 0u; side < 6u; ++side) {
      if (node.entry_face < 6u &&
          !world_render_section_faces_can_see(section.visibility,
                                              node.entry_face, side)) {
        continue;
      }
      const world_render_section_key next_key{
          section.key.x + dx[side],
          section.key.y + dy[side],
          section.key.z + dz[side],
      };
      auto next = mesh_buffers.section_indices.find(next_key);
      if (next == mesh_buffers.section_indices.end() ||
          visited[next->second] != 0u) {
        continue;
      }
      pending.push({next->second, world_render_section_opposite_face(side)});
    }
  }

  if (list.opaque_indices.empty() && list.sprite_indices.empty() &&
      !mesh_buffers.chunks.empty()) {
    append_fallback_draws(mesh_buffers, camera, list);
  }
  return list;
}
