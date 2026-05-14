#include "WorldMeshUpload.h"

#include "Log.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <unordered_map>

namespace {

world_render_section_key key_from_chunk(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return {chunk.chunk_x, chunk.chunk_y, chunk.chunk_z};
}

bool same_section_key(const world_render_section_key &left,
                      const world_render_section_key &right) {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool chunk_mesh_has_geometry(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  return chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
         chunk.sprite_vertex_count != 0u;
}

octaryn_client_chunk_mesh_upload_record retained_chunk_record(
    const octaryn_client_chunk_mesh_upload_record &chunk) {
  octaryn_client_chunk_mesh_upload_record copied = chunk;
  copied.opaque_face_offset = 0u;
  copied.transparent_face_offset = 0u;
  copied.sprite_vertex_offset = 0u;
  copied.opaque_byte_count =
      static_cast<uint64_t>(copied.opaque_face_count) * sizeof(uint64_t);
  copied.transparent_byte_count =
      static_cast<uint64_t>(copied.transparent_face_count) * sizeof(uint64_t);
  copied.sprite_byte_count =
      static_cast<uint64_t>(copied.sprite_vertex_count) * sizeof(uint32_t);
  return copied;
}

void rebuild_chunk_indices(world_mesh_upload_frame &frame) {
  frame.chunk_indices.clear();
  frame.chunk_indices.reserve(frame.chunks.size());
  for (size_t index = 0u; index < frame.chunks.size(); ++index) {
    frame.chunk_indices[key_from_chunk(frame.chunks[index])] = index;
  }
}

void ensure_chunk_indices(world_mesh_upload_frame &frame) {
  if (frame.chunk_indices.size() != frame.chunks.size()) {
    rebuild_chunk_indices(frame);
  }
}

void remove_retained_chunk(world_mesh_upload_frame &frame,
                           const world_render_section_key &key) {
  const auto found = frame.chunk_indices.find(key);
  if (found == frame.chunk_indices.end() || found->second >= frame.chunks.size()) {
    return;
  }
  const size_t index = found->second;
  frame.chunk_indices.erase(found);
  const size_t last_index = frame.chunks.size() - 1u;
  if (index != last_index) {
    frame.chunks[index] = frame.chunks[last_index];
    frame.chunk_indices[key_from_chunk(frame.chunks[index])] = index;
  }
  frame.chunks.pop_back();
}

void upsert_retained_chunk(world_mesh_upload_frame &frame,
                           const octaryn_client_chunk_mesh_upload_record &chunk) {
  const world_render_section_key key = key_from_chunk(chunk);
  const octaryn_client_chunk_mesh_upload_record retained =
      retained_chunk_record(chunk);
  const auto found = frame.chunk_indices.find(key);
  if (found != frame.chunk_indices.end() && found->second < frame.chunks.size()) {
    frame.chunks[found->second] = retained;
    return;
  }
  const size_t index = frame.chunks.size();
  frame.chunks.push_back(retained);
  frame.chunk_indices[key] = index;
}

void append_unloaded_section(world_mesh_upload_frame &frame,
                             const octaryn_client_chunk_mesh_upload_record &chunk) {
  world_render_section_state section{};
  section.key = {chunk.chunk_x, chunk.chunk_y, chunk.chunk_z};
  frame.sections.push_back(section);
}

void compact_section_deltas(world_mesh_upload_frame &frame) {
  constexpr size_t kMinimumCompactSize = 24576u;
  const size_t compact_budget =
      std::max(kMinimumCompactSize, frame.chunks.size() * 12u);
  if (frame.sections.size() <= compact_budget) {
    return;
  }

  std::unordered_map<world_render_section_key, size_t,
                     world_render_section_key_hash>
      latest;
  latest.reserve(frame.sections.size());
  for (size_t index = 0u; index < frame.sections.size(); ++index) {
    latest[frame.sections[index].key] = index;
  }

  std::vector<world_render_section_state> compacted;
  compacted.reserve(latest.size());
  for (size_t index = 0u; index < frame.sections.size(); ++index) {
    const world_render_section_state &section = frame.sections[index];
    const auto found = latest.find(section.key);
    if (found == latest.end() || found->second >= frame.sections.size() ||
        !same_section_key(frame.sections[found->second].key, section.key) ||
        (frame.sections[found->second].flags & kWorldRenderSectionLoaded) == 0u) {
      continue;
    }
    if (found->second == index) {
      compacted.push_back(section);
    }
  }
  frame.sections = std::move(compacted);
}

void update_retained_totals(world_mesh_upload_frame &frame) {
  frame.opaque_bytes = 0u;
  frame.transparent_bytes = 0u;
  frame.sprite_bytes = 0u;
  frame.fluid_blocks = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk : frame.chunks) {
    frame.opaque_bytes += chunk.opaque_byte_count;
    frame.transparent_bytes += chunk.transparent_byte_count;
    frame.sprite_bytes += chunk.sprite_byte_count;
    frame.fluid_blocks += chunk.fluid_block_count;
  }
}

} // namespace

void merge_world_mesh_upload_frame(world_mesh_upload_frame &visible_frame,
                                   const world_mesh_upload_frame &update_frame,
                                   uint64_t frame_index, const char *source) {
  if (update_frame.chunks.empty() && update_frame.sections.empty()) {
    return;
  }

  uint32_t sprite_indices = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    sprite_indices += chunk.sprite_index_count;
  }
  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_drain frame=%" PRIu64
                 " active=1 source=%s chunks=%zu opaque_faces=%zu"
                 " transparent_faces=%zu sprite_vertices=%zu"
                 " sprite_indices=%" PRIu32 " fluid_blocks=%" PRIu32 "\n",
                 frame_index, source, update_frame.chunks.size(),
                 update_frame.opaque_faces.size(),
                 update_frame.transparent_faces.size(),
                 update_frame.sprite_vertices.size(), sprite_indices,
                 update_frame.fluid_blocks);
    std::fflush(octaryn_client_app::g_log);
  }

  ensure_chunk_indices(visible_frame);
  uint32_t removed_chunks = 0u;
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       update_frame.chunks) {
    if (chunk_mesh_has_geometry(chunk)) {
      upsert_retained_chunk(visible_frame, chunk);
    } else {
      remove_retained_chunk(visible_frame, key_from_chunk(chunk));
      append_unloaded_section(visible_frame, chunk);
      ++removed_chunks;
    }
  }
  update_retained_totals(visible_frame);
  visible_frame.sections.insert(visible_frame.sections.end(),
                                update_frame.sections.begin(),
                                update_frame.sections.end());
  compact_section_deltas(visible_frame);

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_retained frame=%" PRIu64
                 " active=1 updates=%zu retained=%zu removed=%" PRIu32
                 " visible_chunks=%zu opaque_faces=%zu transparent_faces=%zu"
                 " sprite_vertices=%zu render_sections=%zu\n",
                 frame_index, update_frame.chunks.size(),
                 visible_frame.chunks.size(), removed_chunks,
                 visible_frame.chunks.size(), visible_frame.opaque_faces.size(),
                 visible_frame.transparent_faces.size(),
                 visible_frame.sprite_vertices.size(),
                 visible_frame.sections.size());
    std::fflush(octaryn_client_app::g_log);
  }
}
