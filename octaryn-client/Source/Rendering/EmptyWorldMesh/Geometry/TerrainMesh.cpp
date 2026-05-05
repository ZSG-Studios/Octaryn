#include "EmptyWorldMesh.h"

#include "ChunkMeshPlan.h"
#include "Log.h"
#include "Packing.h"
#include "View.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <unordered_map>
#include <vector>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::server_chunk_stream_column_record;

namespace {

constexpr uint32_t kUploadRecordVersion = 1u;
constexpr uint32_t kUploadRecordSize = 96u;
constexpr uint32_t kClearTransparentFaces = 1u << 1u;
constexpr uint32_t kClearSpriteVertices = 1u << 2u;
constexpr uint32_t kClearFluidBlocks = 1u << 3u;
constexpr int32_t kEmptyWorldMaxChunkY =
    (kEmptyWorldMaxYExclusive - 1) / kEmptyWorldChunkSize;
constexpr uint16_t kBlockAir = 0u;

struct chunk_key {
  int32_t x;
  int32_t y;
  int32_t z;

  bool operator==(const chunk_key &other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct chunk_key_hash {
  size_t operator()(const chunk_key &key) const {
    uint64_t hash = 1469598103934665603ull;
    hash = (hash ^ static_cast<uint32_t>(key.x)) * 1099511628211ull;
    hash = (hash ^ static_cast<uint32_t>(key.y)) * 1099511628211ull;
    hash = (hash ^ static_cast<uint32_t>(key.z)) * 1099511628211ull;
    return static_cast<size_t>(hash);
  }
};

using face_batches =
    std::unordered_map<chunk_key, std::vector<uint64_t>, chunk_key_hash>;

struct terrain_top_cell {
  int32_t height;
  uint16_t block;
  bool visible;
};

struct terrain_chunk_samples {
  std::array<empty_world_terrain_column,
             (kEmptyWorldChunkSize + 2) * (kEmptyWorldChunkSize + 2)>
      columns{};

  const empty_world_terrain_column &at(int32_t local_x, int32_t local_z) const {
    const int32_t sample_x = local_x + 1;
    const int32_t sample_z = local_z + 1;
    return columns[static_cast<size_t>(sample_z * (kEmptyWorldChunkSize + 2) +
                                       sample_x)];
  }
};

terrain_chunk_samples sample_terrain_chunk(int32_t origin_x, int32_t origin_z) {
  terrain_chunk_samples samples{};
  for (int32_t local_z = -1; local_z <= kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = -1; local_x <= kEmptyWorldChunkSize; ++local_x) {
      samples.columns[static_cast<size_t>(
          (local_z + 1) * (kEmptyWorldChunkSize + 2) + local_x + 1)] =
          empty_world_seed_column(origin_x + local_x, origin_z + local_z);
    }
  }
  return samples;
}

bool chunk_inside_view(int32_t chunk_x, int32_t chunk_z,
                       const chunk_view &view) {
  return chunk_x >= view.origin_x && chunk_x < view.origin_x + view.width &&
         chunk_z >= view.origin_z && chunk_z < view.origin_z + view.width;
}

bool dirty_column_contains(
    const std::vector<empty_world_dirty_column> &dirty_columns, int32_t chunk_x,
    int32_t chunk_z) {
  for (const empty_world_dirty_column &column : dirty_columns) {
    if (column.chunk_x == chunk_x && column.chunk_z == chunk_z) {
      return true;
    }
  }
  return false;
}

bool override_column_contains(const block_lookup &overrides, int32_t chunk_x,
                              int32_t chunk_z) {
  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (floor_div_int32(key.x, kEmptyWorldChunkSize) == chunk_x &&
        floor_div_int32(key.z, kEmptyWorldChunkSize) == chunk_z) {
      return true;
    }
  }
  return false;
}

uint16_t effective_block_at(const block_lookup &overrides, int32_t world_x,
                            int32_t world_y, int32_t world_z) {
  const block_position_key key{world_x, world_y, world_z};
  return empty_world_effective_block(overrides, key);
}

void append_face(face_batches &batches, int32_t world_x, int32_t world_y,
                 int32_t world_z, uint32_t direction, uint32_t span_u,
                 uint32_t span_v, uint16_t block) {
  const chunk_key key{floor_div_int32(world_x, kEmptyWorldChunkSize),
                      floor_div_int32(world_y, kEmptyWorldChunkSize),
                      floor_div_int32(world_z, kEmptyWorldChunkSize)};
  batches[key].push_back(pack_empty_world_block_face_with_layer(
      static_cast<uint32_t>(floor_mod_int32(world_x, kEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_y, kEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_z, kEmptyWorldChunkSize)),
      direction, span_u, span_v,
      empty_world_block_atlas_layer(block, direction)));
}

void append_side_span(face_batches &batches, int32_t world_x, int32_t start_y,
                      int32_t end_y, int32_t world_z, uint32_t direction,
                      uint16_t block) {
  int32_t y = start_y;
  while (y <= end_y) {
    const int32_t chunk_end = std::min(
        end_y, floor_div_int32(y, kEmptyWorldChunkSize) * kEmptyWorldChunkSize +
                   kEmptyWorldChunkSize - 1);
    append_face(batches, world_x, y, world_z, direction, 1u,
                static_cast<uint32_t>(chunk_end - y + 1), block);
    y = chunk_end + 1;
  }
}

bool top_cell_matches(const std::array<terrain_top_cell, 1024> &cells,
                      const std::array<bool, 1024> &used, int32_t x, int32_t z,
                      const terrain_top_cell &seed) {
  const size_t index = static_cast<size_t>(z * kEmptyWorldChunkSize + x);
  const terrain_top_cell &cell = cells[index];
  return !used[index] && cell.visible && cell.height == seed.height &&
         cell.block == seed.block;
}

void append_terrain_top_faces(face_batches &batches,
                              const block_lookup &overrides, bool has_overrides,
                              int32_t origin_x, int32_t origin_z,
                              const terrain_chunk_samples &samples) {
  std::array<terrain_top_cell, 1024> cells{};
  for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
      const int32_t world_x = origin_x + local_x;
      const int32_t world_z = origin_z + local_z;
      const empty_world_terrain_column &column = samples.at(local_x, local_z);
      const uint16_t block =
          has_overrides
              ? effective_block_at(overrides, world_x, column.height, world_z)
              : column.surface;
      cells[static_cast<size_t>(local_z * kEmptyWorldChunkSize + local_x)] =
          terrain_top_cell{column.height, column.surface, block != kBlockAir};
    }
  }

  std::array<bool, 1024> used{};
  for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
    for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
      const size_t index =
          static_cast<size_t>(local_z * kEmptyWorldChunkSize + local_x);
      const terrain_top_cell cell = cells[index];
      if (used[index] || !cell.visible) {
        continue;
      }

      int32_t width = 1;
      while (local_x + width < kEmptyWorldChunkSize &&
             top_cell_matches(cells, used, local_x + width, local_z, cell)) {
        ++width;
      }

      int32_t depth = 1;
      bool can_extend = true;
      while (local_z + depth < kEmptyWorldChunkSize && can_extend) {
        for (int32_t x = 0; x < width; ++x) {
          if (!top_cell_matches(cells, used, local_x + x, local_z + depth,
                                cell)) {
            can_extend = false;
            break;
          }
        }
        if (can_extend) {
          ++depth;
        }
      }

      for (int32_t z = 0; z < depth; ++z) {
        for (int32_t x = 0; x < width; ++x) {
          used[static_cast<size_t>((local_z + z) * kEmptyWorldChunkSize +
                                   local_x + x)] = true;
        }
      }
      append_face(batches, origin_x + local_x, cell.height, origin_z + local_z,
                  4u, static_cast<uint32_t>(width),
                  static_cast<uint32_t>(depth), cell.block);
    }
  }
}

void append_terrain_column_sides(face_batches &batches,
                                 const terrain_chunk_samples &samples,
                                 int32_t origin_x, int32_t origin_z,
                                 int32_t local_x, int32_t local_z) {
  const empty_world_terrain_column &column = samples.at(local_x, local_z);
  const int32_t world_x = origin_x + local_x;
  const int32_t world_z = origin_z + local_z;
  struct neighbor {
    int32_t dx;
    int32_t dz;
    uint32_t direction;
  };
  constexpr std::array<neighbor, 4> neighbors{{
      {0, 1, 0u},
      {0, -1, 1u},
      {1, 0, 2u},
      {-1, 0, 3u},
  }};

  for (const neighbor &side : neighbors) {
    const int32_t neighbor_height =
        samples.at(local_x + side.dx, local_z + side.dz).height;
    if (neighbor_height >= column.height) {
      continue;
    }

    const int32_t fill_start = neighbor_height + 1;
    const int32_t fill_end = column.height - 1;
    if (fill_start <= fill_end) {
      append_side_span(batches, world_x, fill_start, fill_end, world_z,
                       side.direction, column.fill);
    }
    append_face(batches, world_x, column.height, world_z, side.direction, 1u,
                1u, column.surface);
  }
}

void append_override_cube(face_batches &batches, const block_lookup &overrides,
                          int32_t world_x, int32_t world_y, int32_t world_z,
                          uint16_t block) {
  struct neighbor_face {
    int32_t dx;
    int32_t dy;
    int32_t dz;
    uint32_t direction;
  };
  constexpr std::array<neighbor_face, 6> neighbors{{
      {0, 0, 1, 0u},
      {0, 0, -1, 1u},
      {1, 0, 0, 2u},
      {-1, 0, 0, 3u},
      {0, 1, 0, 4u},
      {0, -1, 0, 5u},
  }};

  for (const neighbor_face &neighbor : neighbors) {
    const uint16_t neighbor_block =
        effective_block_at(overrides, world_x + neighbor.dx,
                           world_y + neighbor.dy, world_z + neighbor.dz);
    if (neighbor_block == kBlockAir) {
      append_face(batches, world_x, world_y, world_z, neighbor.direction, 1u,
                  1u, block);
    }
  }
}

void append_air_override_neighbor_faces(face_batches &batches,
                                        const block_lookup &overrides,
                                        const block_position_key &air) {
  struct neighbor_face {
    int32_t dx;
    int32_t dy;
    int32_t dz;
    uint32_t direction;
  };
  constexpr std::array<neighbor_face, 6> neighbors{{
      {0, 0, -1, 0u},
      {0, 0, 1, 1u},
      {-1, 0, 0, 2u},
      {1, 0, 0, 3u},
      {0, -1, 0, 4u},
      {0, 1, 0, 5u},
  }};

  for (const neighbor_face &neighbor : neighbors) {
    const int32_t x = air.x + neighbor.dx;
    const int32_t y = air.y + neighbor.dy;
    const int32_t z = air.z + neighbor.dz;
    const uint16_t block = effective_block_at(overrides, x, y, z);
    if (block != kBlockAir) {
      append_face(batches, x, y, z, neighbor.direction, 1u, 1u, block);
    }
  }
}

void append_upload_chunk(world_mesh_upload_frame &mesh_frame,
                         const chunk_key &key,
                         const std::vector<uint64_t> &faces) {
  if (faces.empty()) {
    return;
  }

  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.version = kUploadRecordVersion;
  chunk.size = kUploadRecordSize;
  chunk.chunk_x = key.x;
  chunk.chunk_y = key.y;
  chunk.chunk_z = key.z;
  chunk.flags =
      kClearTransparentFaces | kClearSpriteVertices | kClearFluidBlocks;
  chunk.opaque_face_offset = mesh_frame.opaque_faces.size();
  chunk.opaque_face_count = static_cast<uint32_t>(faces.size());
  chunk.opaque_byte_count =
      static_cast<uint64_t>(faces.size()) * sizeof(uint64_t);
  mesh_frame.opaque_faces.insert(mesh_frame.opaque_faces.end(), faces.begin(),
                                 faces.end());
  mesh_frame.opaque_bytes += chunk.opaque_byte_count;
  mesh_frame.chunks.push_back(chunk);
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

void append_clear_column(world_mesh_upload_frame &mesh_frame, int32_t chunk_x,
                         int32_t chunk_z) {
  for (int32_t chunk_y = kEmptyWorldMinChunkY; chunk_y <= kEmptyWorldMaxChunkY;
       ++chunk_y) {
    append_clear_chunk(mesh_frame, chunk_x, chunk_y, chunk_z);
  }
}

bool chunk_key_less(const chunk_key &left, const chunk_key &right) {
  if (left.x != right.x) {
    return left.x < right.x;
  }
  if (left.y != right.y) {
    return left.y < right.y;
  }
  return left.z < right.z;
}

} // namespace

void build_empty_world_mesh_frame_from_stream_columns(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const std::vector<server_chunk_stream_column_record> &selected_columns,
    const block_lookup &overrides, const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    world_mesh_upload_frame &mesh_frame) {
  mesh_frame = {};

  const chunk_view stream_view = chunk_view_from_server_stream(stream);
  const chunk_mesh_plan mesh_plan =
      build_chunk_mesh_plan(previous_chunk_view, stream_view,
                            chunk_mesh_plan_default_options(stream_view));
  int32_t previous_min_x = 0;
  int32_t previous_max_x = 0;
  int32_t previous_min_z = 0;
  int32_t previous_max_z = 0;
  if (empty_world_chunk_range(previous_chunk_view, previous_min_x,
                              previous_max_x, previous_min_z, previous_max_z)) {
    for (int32_t chunk_z = previous_min_z; chunk_z < previous_max_z;
         ++chunk_z) {
      for (int32_t chunk_x = previous_min_x; chunk_x < previous_max_x;
           ++chunk_x) {
        if (!chunk_inside_view(chunk_x, chunk_z, stream_view)) {
          append_clear_column(mesh_frame, chunk_x, chunk_z);
        }
      }
    }
  }

  face_batches batches;
  batches.reserve(selected_columns.size());
  for (const server_chunk_stream_column_record &column : selected_columns) {
    const bool was_visible =
        chunk_inside_view(column.chunkX, column.chunkZ, previous_chunk_view);
    const bool dirty =
        dirty_column_contains(dirty_columns, column.chunkX, column.chunkZ);
    const bool column_has_overrides =
        override_column_contains(overrides, column.chunkX, column.chunkZ);
    if (was_visible && !dirty && !column_has_overrides) {
      continue;
    }

    if (was_visible) {
      append_clear_column(mesh_frame, column.chunkX, column.chunkZ);
    }
    const terrain_chunk_samples samples =
        sample_terrain_chunk(column.originX, column.originZ);
    append_terrain_top_faces(batches, overrides, column_has_overrides,
                             column.originX, column.originZ, samples);
    for (int32_t local_z = 0; local_z < kEmptyWorldChunkSize; ++local_z) {
      for (int32_t local_x = 0; local_x < kEmptyWorldChunkSize; ++local_x) {
        append_terrain_column_sides(batches, samples, column.originX,
                                    column.originZ, local_x, local_z);
      }
    }
  }

  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (entry.second == kBlockAir) {
      append_air_override_neighbor_faces(batches, overrides, key);
    } else {
      append_override_cube(batches, overrides, key.x, key.y, key.z,
                           entry.second);
    }
  }

  std::vector<const face_batches::value_type *> sorted_batches;
  sorted_batches.reserve(batches.size());
  for (const auto &entry : batches) {
    sorted_batches.push_back(&entry);
  }
  std::sort(sorted_batches.begin(), sorted_batches.end(),
            [](const auto *left, const auto *right) {
              return chunk_key_less(left->first, right->first);
            });

  mesh_frame.chunks.reserve(batches.size());
  for (const auto *entry : sorted_batches) {
    append_upload_chunk(mesh_frame, entry->first, entry->second);
  }

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(
        octaryn_client_app::g_log,
        "native_empty_chunk_stream active=1 "
        "source=server_seed_memory epoch=%" PRIu64 " render_distance=%" PRIu32
        " columns=%zu loaded=%zu preserved=%zu override_edits=%zu "
        "dirty_columns=%zu visible_chunks=%zu opaque_faces=%zu "
        "unloaded=%zu active_columns=%zu plan_entries=%zu "
        "urgent_jobs=%zu regular_jobs=%zu clear_jobs=%zu "
        "scheduled_urgent=%zu scheduled_regular=%zu\n",
        stream.epoch, stream.radius, selected_columns.size(),
        mesh_plan.summary.loaded_columns, mesh_plan.summary.preserved_columns,
        overrides.size(), dirty_columns.size(), mesh_frame.chunks.size(),
        mesh_frame.opaque_faces.size(), mesh_plan.summary.unloaded_columns,
        mesh_plan.summary.active_columns, mesh_plan.entries.size(),
        mesh_plan.summary.urgent_jobs, mesh_plan.summary.regular_jobs,
        mesh_plan.summary.clear_jobs, mesh_plan.summary.scheduled_urgent_jobs,
        mesh_plan.summary.scheduled_regular_jobs);
    std::fflush(octaryn_client_app::g_log);
  }
}
