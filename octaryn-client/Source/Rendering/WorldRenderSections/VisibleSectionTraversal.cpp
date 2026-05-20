#include "VisibleSectionTraversal.h"

namespace {

void append_retained_draws(const world_mesh_gpu_buffers &mesh_buffers,
                           visible_section_draw_list &list) {
  for (size_t index = 0u; index < mesh_buffers.chunks.size(); ++index) {
    const auto &chunk = mesh_buffers.chunks[index];
    ++list.visited_sections;
    ++list.retained_all;
    if (chunk.record.opaque_face_count != 0u && chunk.opaque_faces != nullptr) {
      list.opaque_indices.push_back(index);
    }
    if (chunk.record.transparent_face_count != 0u &&
        chunk.transparent_faces != nullptr) {
      list.transparent_indices.push_back(index);
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
  (void)camera;
  visible_section_draw_list list{};
  list.total_sections = static_cast<uint32_t>(mesh_buffers.sections.size());
  append_retained_draws(mesh_buffers, list);
  return list;
}
