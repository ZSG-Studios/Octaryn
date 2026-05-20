#include "WorldMeshUpload.h"

namespace {

world_render_section_key key_from_chunk(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return {chunk.chunk_x, chunk.chunk_y, chunk.chunk_z};
}

bool chunk_mesh_has_geometry(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
         chunk.sprite_vertex_count != 0u;
}

bool update_replaces_chunk_with_geometry(
    const world_mesh_upload_frame &update_frame,
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  const world_render_section_key key = key_from_chunk(chunk);
  for (const octaryn_client_chunk_mesh_upload_record &update :
       update_frame.chunks) {
    if (chunk_mesh_has_geometry(update) && key_from_chunk(update) == key) {
      return true;
    }
  }
  return false;
}

void rebuild_chunk_indices(world_mesh_gpu_buffers &buffers) {
  buffers.chunk_indices.clear();
  buffers.chunk_indices.reserve(buffers.chunks.size());
  for (size_t index = 0u; index < buffers.chunks.size(); ++index) {
    buffers.chunk_indices.emplace(key_from_chunk(buffers.chunks[index].record),
                                  index);
  }
}

void remove_section(world_mesh_gpu_buffers &buffers,
                    const world_render_section_key &key) {
  const auto found = buffers.section_indices.find(key);
  if (found == buffers.section_indices.end()) {
    return;
  }
  const size_t index = found->second;
  buffers.section_indices.erase(found);
  if (index >= buffers.sections.size()) {
    return;
  }
  const size_t last_index = buffers.sections.size() - 1u;
  if (index != last_index) {
    buffers.sections[index] = buffers.sections[last_index];
    buffers.section_indices[buffers.sections[index].key] = index;
  }
  buffers.sections.pop_back();
}

void apply_section(world_mesh_gpu_buffers &buffers,
                   const world_render_section_state &section) {
  if ((section.flags & kWorldRenderSectionLoaded) == 0u) {
    remove_section(buffers, section.key);
    return;
  }
  const auto found = buffers.section_indices.find(section.key);
  if (found != buffers.section_indices.end() &&
      found->second < buffers.sections.size()) {
    buffers.sections[found->second] = section;
    return;
  }
  const size_t index = buffers.sections.size();
  buffers.sections.push_back(section);
  buffers.section_indices[section.key] = index;
}

} // namespace

void rebuild_world_mesh_draw_indices(world_mesh_gpu_buffers &buffers) {
  buffers.section_indices.clear();
  buffers.section_indices.reserve(buffers.sections.size());
  for (size_t index = 0u; index < buffers.sections.size(); ++index) {
    const world_render_section_state &section = buffers.sections[index];
    if ((section.flags & kWorldRenderSectionLoaded) != 0u) {
      buffers.section_indices[section.key] = index;
    } else {
      buffers.section_indices.erase(section.key);
    }
  }

  rebuild_chunk_indices(buffers);
}

void apply_world_mesh_draw_index_update(
    world_mesh_gpu_buffers &buffers,
    const world_mesh_upload_frame &update_frame) {
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    if (!chunk_mesh_has_geometry(chunk) &&
        !update_replaces_chunk_with_geometry(update_frame, chunk)) {
      remove_section(buffers, key_from_chunk(chunk));
    }
  }
  for (const world_render_section_state &section : update_frame.sections) {
    apply_section(buffers, section);
  }
}
