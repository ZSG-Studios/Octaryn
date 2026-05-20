#include "TerrainMeshEditClears.h"

#include "Log.h"
#include "Packing.h"

#include <cinttypes>
#include <cstdio>
#include <unordered_set>

namespace {

constexpr uint32_t kUploadRecordVersion = 1u;
constexpr uint32_t kUploadRecordSize = 96u;
constexpr uint32_t kClearTransparentFaces = 1u << 1u;
constexpr uint32_t kClearSpriteVertices = 1u << 2u;
constexpr uint32_t kClearFluidBlocks = 1u << 3u;
constexpr int32_t kEmptyWorldMaxChunkY =
    (kEmptyWorldMaxYExclusive - 1) / kEmptyWorldChunkSize;

uint64_t column_key(int32_t chunk_x, int32_t chunk_z) {
  return static_cast<uint32_t>(chunk_x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(chunk_z)) << 32u);
}

uint64_t section_key(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
  uint64_t value = column_key(chunk_x, chunk_z);
  value ^= static_cast<uint64_t>(static_cast<uint32_t>(chunk_y)) +
           0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
  return value;
}

bool chunk_has_geometry(const octaryn_client_chunk_mesh_upload_record &chunk) {
  return chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
         chunk.sprite_vertex_count != 0u;
}

void append_clear_chunk(world_mesh_upload_frame &mesh_frame, int32_t chunk_x,
                        int32_t chunk_y, int32_t chunk_z) {
  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.version = kUploadRecordVersion;
  chunk.size = kUploadRecordSize;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.chunk_z = chunk_z;
  chunk.flags =
      kClearTransparentFaces | kClearSpriteVertices | kClearFluidBlocks;
  mesh_frame.chunks.push_back(chunk);
}

std::unordered_set<uint64_t> selected_column_keys(
    const std::vector<chunk_mesh_plan_entry> &selected) {
  std::unordered_set<uint64_t> keys;
  keys.reserve(selected.size());
  for (const chunk_mesh_plan_entry &entry : selected) {
    if (entry.action != chunk_mesh_plan_action::clear) {
      keys.insert(column_key(entry.chunk_x, entry.chunk_z));
    }
  }
  return keys;
}

std::unordered_set<uint64_t> replacement_section_keys(
    const world_mesh_upload_frame &replacement_frame) {
  std::unordered_set<uint64_t> keys;
  keys.reserve(replacement_frame.chunks.size());
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       replacement_frame.chunks) {
    if (chunk_has_geometry(chunk)) {
      keys.insert(section_key(chunk.chunk_x, chunk.chunk_y, chunk.chunk_z));
    }
  }
  return keys;
}

bool append_clear_if_replaced(
    world_mesh_upload_frame &mesh_frame, std::unordered_set<uint64_t> &cleared,
    const std::unordered_set<uint64_t> &replacements, int32_t chunk_x,
    int32_t chunk_y, int32_t chunk_z) {
  if (chunk_y < kEmptyWorldMinChunkY || chunk_y > kEmptyWorldMaxChunkY) {
    return true;
  }
  const uint64_t key = section_key(chunk_x, chunk_y, chunk_z);
  if (!replacements.contains(key)) {
    return false;
  }
  if (cleared.insert(key).second) {
    append_clear_chunk(mesh_frame, chunk_x, chunk_y, chunk_z);
  }
  return true;
}

} // namespace

void append_guarded_override_section_clears(
    world_mesh_upload_frame &mesh_frame,
    const octaryn_client_app::block_lookup &overrides,
    const std::vector<chunk_mesh_plan_entry> &selected,
    const world_mesh_upload_frame &replacement_frame) {
  const std::unordered_set<uint64_t> selected_columns =
      selected_column_keys(selected);
  const std::unordered_set<uint64_t> replacement_sections =
      replacement_section_keys(replacement_frame);
  std::unordered_set<uint64_t> cleared_sections;
  uint32_t skipped = 0u;

  for (const auto &entry : overrides) {
    const octaryn_client_app::block_position_key &key = entry.first;
    const int32_t chunk_x = floor_div_int32(key.x, kEmptyWorldChunkSize);
    const int32_t chunk_y = floor_div_int32(key.y, kEmptyWorldChunkSize);
    const int32_t chunk_z = floor_div_int32(key.z, kEmptyWorldChunkSize);
    if (!selected_columns.contains(column_key(chunk_x, chunk_z))) {
      continue;
    }
    if (!append_clear_if_replaced(mesh_frame, cleared_sections,
                                  replacement_sections, chunk_x, chunk_y,
                                  chunk_z)) {
      ++skipped;
    }
    if (key.y == chunk_y * kEmptyWorldChunkSize) {
      skipped += append_clear_if_replaced(mesh_frame, cleared_sections,
                                          replacement_sections, chunk_x,
                                          chunk_y - 1, chunk_z)
                     ? 0u
                     : 1u;
    } else if (key.y == (chunk_y + 1) * kEmptyWorldChunkSize - 1) {
      skipped += append_clear_if_replaced(mesh_frame, cleared_sections,
                                          replacement_sections, chunk_x,
                                          chunk_y + 1, chunk_z)
                     ? 0u
                     : 1u;
    }
  }

  if (skipped != 0u && octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "live_chunk_mesh_clear_guard active=1 skipped=%" PRIu32
                 " reason=no_replacement_section selected=%zu"
                 " replacements=%zu overrides=%zu\n",
                 skipped, selected.size(), replacement_sections.size(),
                 overrides.size());
    std::fflush(octaryn_client_app::g_log);
  }
}
