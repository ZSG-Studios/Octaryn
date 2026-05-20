#include "WorldMeshRetainedColumns.h"

#include <unordered_set>

namespace octaryn_client_app {

namespace {

uint64_t retained_column_key(int32_t chunk_x, int32_t chunk_z) {
  return static_cast<uint32_t>(chunk_x) |
         (static_cast<uint64_t>(static_cast<uint32_t>(chunk_z)) << 32u);
}

} // namespace

std::vector<empty_world_retained_column> retained_columns_from_frame(
    const world_mesh_upload_frame &frame) {
  std::vector<empty_world_retained_column> columns;
  columns.reserve(frame.chunks.size());
  std::unordered_set<uint64_t> seen;
  seen.reserve(frame.chunks.size());
  for (const octaryn_client_chunk_mesh_upload_record &chunk : frame.chunks) {
    if (chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
        chunk.sprite_vertex_count != 0u || chunk.fluid_block_count != 0u) {
      if (!seen.insert(retained_column_key(chunk.chunk_x, chunk.chunk_z))
               .second) {
        continue;
      }
      columns.push_back({chunk.chunk_x, chunk.chunk_z});
    }
  }
  return columns;
}

} // namespace octaryn_client_app
