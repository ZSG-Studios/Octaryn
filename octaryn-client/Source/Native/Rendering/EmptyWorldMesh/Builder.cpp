#include "EmptyWorldMesh.h"

#include "Log.h"
#include "Packing.h"
#include "View.h"

#include <array>
#include <cstdio>

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;

namespace {

constexpr uint32_t kClientChunkMeshUploadRecordVersion = 1u;
constexpr uint32_t kClientChunkMeshUploadRecordSize = 96u;
constexpr uint32_t kClientChunkMeshClearTransparentFacesFlag = 1u << 1u;
constexpr uint32_t kClientChunkMeshClearSpriteVerticesFlag = 1u << 2u;
constexpr uint32_t kClientChunkMeshClearFluidBlocksFlag = 1u << 3u;

void append_empty_world_chunk_faces(world_mesh_upload_frame &mesh_frame,
                                    int32_t chunk_x, int32_t chunk_y,
                                    int32_t chunk_z,
                                    const std::vector<uint64_t> &faces) {
  if (faces.empty()) {
    return;
  }

  octaryn_client_chunk_mesh_upload_record chunk{};
  chunk.version = kClientChunkMeshUploadRecordVersion;
  chunk.size = kClientChunkMeshUploadRecordSize;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.chunk_z = chunk_z;
  chunk.flags = kClientChunkMeshClearTransparentFacesFlag |
                kClientChunkMeshClearSpriteVerticesFlag |
                kClientChunkMeshClearFluidBlocksFlag;
  chunk.opaque_face_offset = mesh_frame.opaque_faces.size();
  chunk.opaque_face_count = static_cast<uint32_t>(faces.size());
  chunk.opaque_byte_count =
      static_cast<uint64_t>(faces.size()) * sizeof(uint64_t);

  mesh_frame.opaque_faces.insert(mesh_frame.opaque_faces.end(), faces.begin(),
                                 faces.end());
  mesh_frame.opaque_bytes += chunk.opaque_byte_count;
  mesh_frame.chunks.push_back(chunk);
}

void append_empty_world_cube_faces(std::vector<uint64_t> &faces,
                                   uint32_t local_x, uint32_t local_y,
                                   uint32_t local_z) {
  for (uint32_t direction = 0u; direction < 6u; ++direction) {
    faces.push_back(pack_empty_world_block_face(local_x, local_y, local_z,
                                                direction, 1u, 1u));
  }
}

void append_empty_world_block_face(world_mesh_upload_frame &mesh_frame,
                                   int32_t world_x, int32_t world_y,
                                   int32_t world_z, uint32_t direction) {
  std::vector<uint64_t> faces;
  faces.push_back(pack_empty_world_block_face(
      static_cast<uint32_t>(floor_mod_int32(world_x, kEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_y, kEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_z, kEmptyWorldChunkSize)),
      direction, 1u, 1u));
  append_empty_world_chunk_faces(
      mesh_frame, floor_div_int32(world_x, kEmptyWorldChunkSize),
      floor_div_int32(world_y, kEmptyWorldChunkSize),
      floor_div_int32(world_z, kEmptyWorldChunkSize), faces);
}

void append_empty_world_exposed_air_faces(world_mesh_upload_frame &mesh_frame,
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
    const block_position_key solid{
        air.x + neighbor.dx,
        air.y + neighbor.dy,
        air.z + neighbor.dz,
    };
    if (empty_world_effective_block(overrides, solid) == 0u) {
      continue;
    }

    append_empty_world_block_face(mesh_frame, solid.x, solid.y, solid.z,
                                  neighbor.direction);
  }
}

} // namespace

void build_empty_world_mesh_frame(
    const octaryn_client_chunk_view &chunk_view,
    const octaryn_client_chunk_view &previous_chunk_view,
    const block_lookup &overrides, world_mesh_upload_frame &mesh_frame) {
  mesh_frame = {};
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!empty_world_chunk_range(chunk_view, min_chunk_x, max_chunk_x,
                               min_chunk_z, max_chunk_z)) {
    if (octaryn_client_app::g_log != nullptr) {
      const size_t previous_count =
          empty_world_chunk_count(previous_chunk_view);
      std::fprintf(octaryn_client_app::g_log,
                   "native_empty_chunk_stream active=1 loaded=0 "
                   "preserved=0 unloaded=%zu reason=outside_bounds "
                   "render_distance=%d source=client_native_unbounded\n",
                   previous_count, chunk_view.width / 2);
      std::fflush(octaryn_client_app::g_log);
    }
    return;
  }

  const size_t chunk_count = static_cast<size_t>(max_chunk_x - min_chunk_x) *
                             static_cast<size_t>(max_chunk_z - min_chunk_z);
  mesh_frame.chunks.reserve(chunk_count);
  mesh_frame.opaque_faces.reserve(chunk_count);

  for (int32_t chunk_z = min_chunk_z; chunk_z < max_chunk_z; ++chunk_z) {
    for (int32_t chunk_x = min_chunk_x; chunk_x < max_chunk_x; ++chunk_x) {
      for (int32_t chunk_y = kEmptyWorldMinChunkY; chunk_y <= kEmptyWorldChunkY;
           ++chunk_y) {
        std::vector<uint64_t> volume_faces;
        if (chunk_y == kEmptyWorldMinChunkY) {
          volume_faces.push_back(pack_empty_world_block_face(
              0u, 0u, 0u, 5u, kEmptyWorldChunkSize, kEmptyWorldChunkSize));
        }
        if (chunk_x == min_chunk_x) {
          volume_faces.push_back(pack_empty_world_block_face(
              0u, 0u, 0u, 3u, kEmptyWorldChunkSize, kEmptyWorldChunkSize));
        }
        if (chunk_x == max_chunk_x - 1) {
          volume_faces.push_back(pack_empty_world_block_face(
              kEmptyWorldChunkSize - 1u, 0u, 0u, 2u, kEmptyWorldChunkSize,
              kEmptyWorldChunkSize));
        }
        if (chunk_z == min_chunk_z) {
          volume_faces.push_back(pack_empty_world_block_face(
              0u, 0u, 0u, 1u, kEmptyWorldChunkSize, kEmptyWorldChunkSize));
        }
        if (chunk_z == max_chunk_z - 1) {
          volume_faces.push_back(pack_empty_world_block_face(
              0u, 0u, kEmptyWorldChunkSize - 1u, 0u, kEmptyWorldChunkSize,
              kEmptyWorldChunkSize));
        }
        append_empty_world_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                       volume_faces);
      }

      std::array<bool, kEmptyWorldChunkSize * kEmptyWorldChunkSize>
          hidden_top{};
      bool has_surface_override = false;
      for (const auto &entry : overrides) {
        const block_position_key &key = entry.first;
        if (key.y != -1 ||
            floor_div_int32(key.x, kEmptyWorldChunkSize) != chunk_x ||
            floor_div_int32(key.z, kEmptyWorldChunkSize) != chunk_z) {
          continue;
        }

        const int32_t local_x = floor_mod_int32(key.x, kEmptyWorldChunkSize);
        const int32_t local_z = floor_mod_int32(key.z, kEmptyWorldChunkSize);
        hidden_top[static_cast<size_t>(local_z * kEmptyWorldChunkSize +
                                       local_x)] = entry.second == 0u;
        has_surface_override = true;
      }

      std::vector<uint64_t> faces;
      if (!has_surface_override) {
        faces.push_back(pack_empty_world_block_face(0u, kEmptyWorldLocalY, 0u,
                                                    4u, kEmptyWorldChunkSize,
                                                    kEmptyWorldChunkSize));
      } else {
        faces.reserve(kEmptyWorldChunkSize * kEmptyWorldChunkSize);
        for (uint32_t local_z = 0u; local_z < kEmptyWorldChunkSize; ++local_z) {
          for (uint32_t local_x = 0u; local_x < kEmptyWorldChunkSize;
               ++local_x) {
            if (hidden_top[static_cast<size_t>(local_z * kEmptyWorldChunkSize +
                                               local_x)]) {
              continue;
            }

            faces.push_back(pack_empty_world_block_face(
                local_x, kEmptyWorldLocalY, local_z, 4u, 1u, 1u));
          }
        }
      }

      append_empty_world_chunk_faces(mesh_frame, chunk_x, kEmptyWorldChunkY,
                                     chunk_z, faces);
    }
  }

  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (entry.second == 0u && empty_world_generated_block(key) != 0u) {
      append_empty_world_exposed_air_faces(mesh_frame, overrides, key);
      continue;
    }

    if (entry.second == 0u || key.y < 0 || key.y >= kEmptyWorldMaxYExclusive) {
      continue;
    }

    const int32_t chunk_x = floor_div_int32(key.x, kEmptyWorldChunkSize);
    const int32_t chunk_y = floor_div_int32(key.y, kEmptyWorldChunkSize);
    const int32_t chunk_z = floor_div_int32(key.z, kEmptyWorldChunkSize);
    if (chunk_x < min_chunk_x || chunk_x >= max_chunk_x ||
        chunk_z < min_chunk_z || chunk_z >= max_chunk_z) {
      continue;
    }

    std::vector<uint64_t> faces;
    append_empty_world_cube_faces(
        faces,
        static_cast<uint32_t>(floor_mod_int32(key.x, kEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.y, kEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.z, kEmptyWorldChunkSize)));
    append_empty_world_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                   faces);
  }

  if (octaryn_client_app::g_log != nullptr) {
    const size_t previous_count = empty_world_chunk_count(previous_chunk_view);
    const size_t preserved =
        empty_world_chunk_overlap(previous_chunk_view, chunk_view);
    const size_t loaded = mesh_frame.chunks.size() - preserved;
    const size_t unloaded = previous_count - preserved;
    std::fprintf(octaryn_client_app::g_log,
                 "native_empty_chunk_stream active=1 source=client_native "
                 "render_distance=%d mode=unbounded_flat y=0 "
                 "loaded=%zu preserved=%zu unloaded=%zu visible_chunks=%zu "
                 "override_edits=%zu opaque_faces=%zu\n",
                 chunk_view.width / 2, loaded, preserved, unloaded,
                 mesh_frame.chunks.size(), overrides.size(),
                 mesh_frame.opaque_faces.size());
    std::fflush(octaryn_client_app::g_log);
  }
}
