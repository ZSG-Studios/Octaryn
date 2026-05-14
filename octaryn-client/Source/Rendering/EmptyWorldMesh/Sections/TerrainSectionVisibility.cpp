#include "TerrainSectionVisibility.h"

#include "EmptyWorldMesh.h"
#include "Packing.h"

#include <algorithm>
#include <cstdint>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::server_chunk_stream_column_record;

namespace {

constexpr int32_t kMaxChunkY =
    (kEmptyWorldMaxYExclusive - 1) / kWorldRenderSectionSize;

bool override_touches_section(const block_lookup &overrides, int32_t section_x,
                              int32_t section_y, int32_t section_z) {
  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (floor_div_int32(key.x, kWorldRenderSectionSize) == section_x &&
        floor_div_int32(key.y, kWorldRenderSectionSize) == section_y &&
        floor_div_int32(key.z, kWorldRenderSectionSize) == section_z) {
      return true;
    }
  }
  return false;
}

void seed_column_height_range(const server_chunk_stream_column_record &column,
                              int32_t &min_height, int32_t &max_height) {
  min_height = kEmptyWorldMaxYExclusive;
  max_height = kEmptyWorldMinY;
  for (int32_t local_z = 0; local_z < kWorldRenderSectionSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kWorldRenderSectionSize; ++local_x) {
      const empty_world_terrain_column seed =
          empty_world_seed_column(column.originX + local_x,
                                  column.originZ + local_z);
      min_height = std::min(min_height, seed.height);
      max_height = std::max(max_height, seed.height);
    }
  }
}

world_render_section_state make_section_state(
    const server_chunk_stream_column_record &column, int32_t section_y,
    int32_t min_height, int32_t max_height, const block_lookup &overrides) {
  const int32_t section_min_y = section_y * kWorldRenderSectionSize;
  const int32_t section_max_y = section_min_y + kWorldRenderSectionSize - 1;
  const bool has_override =
      override_touches_section(overrides, column.chunkX, section_y,
                               column.chunkZ);

  world_render_section_state state{};
  state.key = {column.chunkX, section_y, column.chunkZ};
  state.flags = kWorldRenderSectionLoaded;
  if (!has_override && section_max_y <= min_height) {
    state.flags |= kWorldRenderSectionSolid;
    state.visibility = world_render_section_visibility_none();
    return state;
  }
  if (!has_override && section_min_y > max_height) {
    state.flags |= kWorldRenderSectionEmpty;
  }
  state.visibility = world_render_section_visibility_all();
  return state;
}

} // namespace

void append_empty_world_render_section_states(
    const std::vector<server_chunk_stream_column_record> &columns,
    const block_lookup &overrides, world_mesh_upload_frame &mesh_frame) {
  mesh_frame.sections.reserve(mesh_frame.sections.size() +
                              columns.size() * 16u);
  for (const server_chunk_stream_column_record &column : columns) {
    int32_t min_height = 0;
    int32_t max_height = 0;
    seed_column_height_range(column, min_height, max_height);
    for (int32_t section_y = kEmptyWorldMinChunkY; section_y <= kMaxChunkY;
         ++section_y) {
      mesh_frame.sections.push_back(make_section_state(
          column, section_y, min_height, max_height, overrides));
    }
  }
}
