#include "TerrainMeshCoverageAudit.h"

#include "EmptyWorldMesh.h"
#include "Log.h"
#include "Packing.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::server_chunk_stream_column_record;

namespace {

constexpr uint16_t kBlockAir = 0u;
constexpr uint32_t kDirectionUp = 4u;

bool mesh_audit_enabled() {
  const char *value = std::getenv("OCTARYN_CLIENT_MESH_AUDIT");
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

bool chunk_matches(const octaryn_client_chunk_mesh_upload_record &chunk,
                   int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
  return chunk.chunk_x == chunk_x && chunk.chunk_y == chunk_y &&
         chunk.chunk_z == chunk_z;
}

void mark_top_face_coverage(std::array<bool, 1024> &covered, uint64_t face) {
  if (unpack_empty_world_face_direction(face) != kDirectionUp) {
    return;
  }
  const uint32_t start_x = unpack_empty_world_face_x(face);
  const uint32_t start_z = unpack_empty_world_face_z(face);
  const uint32_t span_x = unpack_empty_world_face_span_u(face);
  const uint32_t span_z = unpack_empty_world_face_span_v(face);
  for (uint32_t z = start_z; z < start_z + span_z && z < 32u; ++z) {
    for (uint32_t x = start_x; x < start_x + span_x && x < 32u; ++x) {
      covered[static_cast<size_t>(z * 32u + x)] = true;
    }
  }
}

std::array<bool, 1024> covered_surface_cells(
    const world_mesh_upload_frame &mesh_frame, int32_t chunk_x,
    int32_t section_y, int32_t chunk_z) {
  std::array<bool, 1024> covered{};
  for (const octaryn_client_chunk_mesh_upload_record &chunk :
       mesh_frame.chunks) {
    if (!chunk_matches(chunk, chunk_x, section_y, chunk_z)) {
      continue;
    }
    for (uint32_t face = 0u; face < chunk.opaque_face_count; ++face) {
      const size_t index =
          static_cast<size_t>(chunk.opaque_face_offset + face);
      if (index < mesh_frame.opaque_faces.size()) {
        mark_top_face_coverage(covered, mesh_frame.opaque_faces[index]);
      }
    }
    for (uint32_t face = 0u; face < chunk.transparent_face_count; ++face) {
      const size_t index =
          static_cast<size_t>(chunk.transparent_face_offset + face);
      if (index < mesh_frame.transparent_faces.size()) {
        mark_top_face_coverage(covered, mesh_frame.transparent_faces[index]);
      }
    }
  }
  return covered;
}

uint32_t count_missing_surface_cells(
    const server_chunk_stream_column_record &column, const block_lookup &overrides,
    const world_mesh_upload_frame &mesh_frame, int32_t &first_x,
    int32_t &first_y, int32_t &first_z) {
  uint32_t missing = 0u;
  std::array<std::array<bool, 1024>, 16> coverage_by_section{};
  std::array<bool, 16> section_loaded{};
  for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
      const int32_t world_x = column.originX + local_x;
      const int32_t world_z = column.originZ + local_z;
      const empty_world_terrain_column seed =
          empty_world_seed_column(world_x, world_z);
      const uint16_t block = empty_world_effective_block(
          overrides, block_position_key{world_x, seed.height, world_z});
      if (block == kBlockAir) {
        continue;
      }
      const int32_t section_y = floor_div_int32(seed.height, kEmptyWorldChunkSize);
      const size_t section_index =
          static_cast<size_t>(section_y - kEmptyWorldMinChunkY);
      if (section_index >= coverage_by_section.size()) {
        continue;
      }
      if (!section_loaded[section_index]) {
        coverage_by_section[section_index] = covered_surface_cells(
            mesh_frame, column.chunkX, section_y, column.chunkZ);
        section_loaded[section_index] = true;
      }
      if (!coverage_by_section[section_index][static_cast<size_t>(
              local_z * kEmptyWorldChunkSize + local_x)]) {
        if (missing == 0u) {
          first_x = world_x;
          first_y = seed.height;
          first_z = world_z;
        }
        ++missing;
      }
    }
  }
  return missing;
}

} // namespace

void audit_terrain_mesh_surface_coverage(
    const std::vector<server_chunk_stream_column_record> &columns,
    const block_lookup &overrides, const world_mesh_upload_frame &mesh_frame) {
  if (octaryn_client_app::g_log == nullptr || columns.empty()) {
    return;
  }
  if (!mesh_audit_enabled()) {
    return;
  }

  uint32_t missing = 0u;
  int32_t first_x = 0;
  int32_t first_y = 0;
  int32_t first_z = 0;
  for (const server_chunk_stream_column_record &column : columns) {
    int32_t column_first_x = 0;
    int32_t column_first_y = 0;
    int32_t column_first_z = 0;
    const uint32_t column_missing = count_missing_surface_cells(
        column, overrides, mesh_frame, column_first_x, column_first_y,
        column_first_z);
    if (missing == 0u && column_missing != 0u) {
      first_x = column_first_x;
      first_y = column_first_y;
      first_z = column_first_z;
    }
    missing += column_missing;
  }
  if (missing == 0u) {
    return;
  }

  std::fprintf(octaryn_client_app::g_log,
               "live_terrain_mesh_surface_coverage active=1 missing=%" PRIu32
               " first_missing=(%d,%d,%d) columns=%zu chunks=%zu"
               " opaque_faces=%zu transparent_faces=%zu overrides=%zu\n",
               missing, first_x, first_y, first_z, columns.size(),
               mesh_frame.chunks.size(), mesh_frame.opaque_faces.size(),
               mesh_frame.transparent_faces.size(), overrides.size());
  std::fflush(octaryn_client_app::g_log);
}
