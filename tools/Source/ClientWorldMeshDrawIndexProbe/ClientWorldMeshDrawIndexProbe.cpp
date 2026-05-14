#include "WorldMeshUpload.h"

#include <cstdio>
#include <string_view>

namespace {

world_render_section_key key(int32_t x, int32_t y, int32_t z) {
  return {x, y, z};
}

world_render_section_state loaded_section(const world_render_section_key &key) {
  world_render_section_state section{};
  section.key = key;
  section.flags = kWorldRenderSectionLoaded;
  return section;
}

octaryn_client_chunk_mesh_upload_record chunk_record(
    const world_render_section_key &key, uint32_t opaque_faces) {
  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.chunk_x = key.x;
  chunk.chunk_y = key.y;
  chunk.chunk_z = key.z;
  chunk.opaque_face_count = opaque_faces;
  return chunk;
}

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_equal(std::string_view label, size_t actual, size_t expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %zu, got %zu\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool has_section(const world_mesh_gpu_buffers &buffers,
                 const world_render_section_key &section_key) {
  const auto found = buffers.section_indices.find(section_key);
  return found != buffers.section_indices.end() &&
         found->second < buffers.sections.size() &&
         buffers.sections[found->second].key == section_key;
}

bool validate_clear_removes_missing_section() {
  const world_render_section_key section_key = key(4, 0, 7);
  world_mesh_gpu_buffers buffers{};
  buffers.sections.push_back(loaded_section(section_key));
  rebuild_world_mesh_draw_indices(buffers);

  world_mesh_upload_frame update{};
  update.chunks.push_back(chunk_record(section_key, 0u));
  apply_world_mesh_draw_index_update(buffers, update);

  bool ok = true;
  ok &= expect_equal("pure clear section count", buffers.sections.size(), 0u);
  ok &= expect_true("pure clear section index removed",
                    !has_section(buffers, section_key));
  return ok;
}

bool validate_rebuilt_edit_section_survives_same_frame_clear() {
  const world_render_section_key section_key = key(8, 0, -3);
  world_mesh_gpu_buffers buffers{};
  buffers.sections.push_back(loaded_section(section_key));
  rebuild_world_mesh_draw_indices(buffers);

  world_mesh_upload_frame update{};
  update.chunks.push_back(chunk_record(section_key, 0u));
  update.sections.push_back(loaded_section(section_key));
  apply_world_mesh_draw_index_update(buffers, update);

  bool ok = true;
  ok &= expect_equal("edited section count", buffers.sections.size(), 1u);
  ok &= expect_true("edited rebuilt section retained",
                    has_section(buffers, section_key));
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_clear_removes_missing_section();
  ok &= validate_rebuilt_edit_section_survives_same_frame_clear();
  return ok ? 0 : 1;
}
