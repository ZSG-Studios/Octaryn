#include "WorldMeshRetainedColumns.h"

namespace octaryn_client_app {

std::vector<empty_world_retained_column> retained_columns_from_frame(
    const world_mesh_upload_frame &frame) {
  std::vector<empty_world_retained_column> columns;
  columns.reserve(frame.chunks.size());
  for (const octaryn_client_chunk_mesh_upload_record &chunk : frame.chunks) {
    if (chunk.opaque_face_count != 0u || chunk.transparent_face_count != 0u ||
        chunk.sprite_vertex_count != 0u || chunk.fluid_block_count != 0u) {
      columns.push_back({chunk.chunk_x, chunk.chunk_z});
    }
  }
  return columns;
}

} // namespace octaryn_client_app
