#include "octaryn_client_native_empty_world_mesh.h"

#include "octaryn_client_app_log.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>

namespace {

constexpr int32_t kNativeEmptyWorldChunkSize = 32;
constexpr int32_t kNativeEmptyWorldMinY = -256;
constexpr int32_t kNativeEmptyWorldMaxYExclusive = 256;
constexpr int32_t kNativeEmptyWorldMinChunkY = -8;
constexpr int32_t kNativeEmptyWorldChunkY = -1;
constexpr int32_t kNativeEmptyWorldLocalY = 31;
constexpr uint32_t kClientChunkMeshUploadRecordVersion = 1u;
constexpr uint32_t kClientChunkMeshUploadRecordSize = 96u;
constexpr uint32_t kClientChunkMeshClearTransparentFacesFlag = 1u << 1u;
constexpr uint32_t kClientChunkMeshClearSpriteVerticesFlag = 1u << 2u;
constexpr uint32_t kClientChunkMeshClearFluidBlocksFlag = 1u << 3u;
constexpr uint32_t kPackedFaceXOffset = 0u;
constexpr uint32_t kPackedFaceYOffset = 5u;
constexpr uint32_t kPackedFaceZOffset = 13u;
constexpr uint32_t kPackedFaceDirectionOffset = 18u;
constexpr uint32_t kPackedFaceSpanUOffset = 21u;
constexpr uint32_t kPackedFaceSpanVOffset = 29u;
constexpr uint32_t kPackedFaceAtlasLayerOffset = 37u;
constexpr uint32_t kPackedFaceOcclusionOffset = 43u;
constexpr uint32_t kPackedFaceChunkSlotOffset = 44u;
constexpr uint32_t kPackedFaceWaterLevelOffset = 57u;
constexpr uint32_t kPackedFaceWaterFlagOffset = 60u;
constexpr uint32_t kPackedFaceWaterBaseHeightOffset = 61u;
constexpr uint64_t kPackedFaceUnsetChunkSlot = 0x1fffu;

using octaryn_client_app::block_lookup;
using octaryn_client_app::block_position_key;
using octaryn_client_app::has_block_override;
using octaryn_client_app::server_chunk_stream_file;
using octaryn_client_app::world_block_record;

uint64_t pack_native_empty_face_field(uint64_t packed, uint64_t value,
                                      uint32_t offset, uint64_t mask) {
  return packed | ((value & mask) << offset);
}

uint64_t pack_native_empty_block_face(uint32_t x, uint32_t y, uint32_t z,
                                      uint32_t direction, uint32_t span_u,
                                      uint32_t span_v) {
  uint64_t packed = 0u;
  packed = pack_native_empty_face_field(packed, x, kPackedFaceXOffset, 0x1fu);
  packed = pack_native_empty_face_field(packed, y, kPackedFaceYOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, z, kPackedFaceZOffset, 0x1fu);
  packed = pack_native_empty_face_field(packed, direction,
                                        kPackedFaceDirectionOffset, 0x7u);
  packed = pack_native_empty_face_field(packed, span_u - 1u,
                                        kPackedFaceSpanUOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, span_v - 1u,
                                        kPackedFaceSpanVOffset, 0xffu);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceAtlasLayerOffset, 0x3fu);
  packed =
      pack_native_empty_face_field(packed, 1u, kPackedFaceOcclusionOffset, 0x1u);
  packed = pack_native_empty_face_field(
      packed, kPackedFaceUnsetChunkSlot, kPackedFaceChunkSlotOffset, 0x1fffu);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceWaterLevelOffset, 0x7u);
  packed =
      pack_native_empty_face_field(packed, 0u, kPackedFaceWaterFlagOffset, 0x1u);
  packed = pack_native_empty_face_field(packed, 0u,
                                        kPackedFaceWaterBaseHeightOffset, 0x7u);
  return packed;
}

int32_t floor_div_int32(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

int32_t floor_mod_int32(int32_t value, int32_t divisor) {
  const int32_t result = value % divisor;
  return result < 0 ? result + divisor : result;
}

void append_native_empty_chunk_faces(world_mesh_upload_frame &mesh_frame,
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

void append_native_empty_cube_faces(std::vector<uint64_t> &faces,
                                    uint32_t local_x, uint32_t local_y,
                                    uint32_t local_z) {
  for (uint32_t direction = 0u; direction < 6u; ++direction) {
    faces.push_back(pack_native_empty_block_face(local_x, local_y, local_z,
                                                direction, 1u, 1u));
  }
}

void append_native_empty_world_block_face(world_mesh_upload_frame &mesh_frame,
                                          int32_t world_x, int32_t world_y,
                                          int32_t world_z,
                                          uint32_t direction) {
  std::vector<uint64_t> faces;
  faces.push_back(pack_native_empty_block_face(
      static_cast<uint32_t>(floor_mod_int32(world_x, kNativeEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_y, kNativeEmptyWorldChunkSize)),
      static_cast<uint32_t>(floor_mod_int32(world_z, kNativeEmptyWorldChunkSize)),
      direction, 1u, 1u));
  append_native_empty_chunk_faces(
      mesh_frame, floor_div_int32(world_x, kNativeEmptyWorldChunkSize),
      floor_div_int32(world_y, kNativeEmptyWorldChunkSize),
      floor_div_int32(world_z, kNativeEmptyWorldChunkSize), faces);
}

void append_native_empty_exposed_air_faces(world_mesh_upload_frame &mesh_frame,
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
    if (native_empty_effective_block(overrides, solid) == 0u) {
      continue;
    }

    append_native_empty_world_block_face(mesh_frame, solid.x, solid.y, solid.z,
                                         neighbor.direction);
  }
}

bool native_empty_world_chunk_range(
    const octaryn_client_chunk_view &chunk_view, int32_t &min_chunk_x,
    int32_t &max_chunk_x, int32_t &min_chunk_z, int32_t &max_chunk_z) {
  min_chunk_x = chunk_view.origin_x;
  max_chunk_x = chunk_view.origin_x + chunk_view.width;
  min_chunk_z = chunk_view.origin_z;
  max_chunk_z = chunk_view.origin_z + chunk_view.width;
  return min_chunk_x < max_chunk_x && min_chunk_z < max_chunk_z;
}

size_t native_empty_world_chunk_count(
    const octaryn_client_chunk_view &chunk_view) {
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!native_empty_world_chunk_range(chunk_view, min_chunk_x, max_chunk_x,
                                      min_chunk_z, max_chunk_z)) {
    return 0u;
  }
  return static_cast<size_t>(max_chunk_x - min_chunk_x) *
         static_cast<size_t>(max_chunk_z - min_chunk_z);
}

size_t native_empty_world_chunk_overlap(
    const octaryn_client_chunk_view &left,
    const octaryn_client_chunk_view &right) {
  int32_t left_min_x = 0;
  int32_t left_max_x = 0;
  int32_t left_min_z = 0;
  int32_t left_max_z = 0;
  int32_t right_min_x = 0;
  int32_t right_max_x = 0;
  int32_t right_min_z = 0;
  int32_t right_max_z = 0;
  if (!native_empty_world_chunk_range(left, left_min_x, left_max_x, left_min_z,
                                      left_max_z) ||
      !native_empty_world_chunk_range(right, right_min_x, right_max_x,
                                      right_min_z, right_max_z)) {
    return 0u;
  }
  const int32_t min_x = std::max(left_min_x, right_min_x);
  const int32_t max_x = std::min(left_max_x, right_max_x);
  const int32_t min_z = std::max(left_min_z, right_min_z);
  const int32_t max_z = std::min(left_max_z, right_max_z);
  if (min_x >= max_x || min_z >= max_z) {
    return 0u;
  }
  return static_cast<size_t>(max_x - min_x) *
         static_cast<size_t>(max_z - min_z);
}

} // namespace

uint16_t native_empty_generated_block(const block_position_key &key) {
  return key.y >= kNativeEmptyWorldMinY &&
                 key.y < 0 &&
                 key.y < kNativeEmptyWorldMaxYExclusive
             ? 1u
             : 0u;
}

uint16_t native_empty_effective_block(const block_lookup &overrides,
                                      const block_position_key &key) {
  uint16_t block = 0u;
  return has_block_override(overrides, key, block)
             ? block
             : native_empty_generated_block(key);
}

void apply_native_empty_overrides_from_records(
    const std::vector<world_block_record> &records, block_lookup &overrides) {
  for (const world_block_record &record : records) {
    if (record.y < kNativeEmptyWorldMinY ||
        record.y >= kNativeEmptyWorldMaxYExclusive) {
      continue;
    }

    overrides[block_position_key{record.x, record.y, record.z}] = record.block;
  }
}

bool same_chunk_view(const octaryn_client_chunk_view &left,
                     const octaryn_client_chunk_view &right) {
  return left.origin_x == right.origin_x && left.origin_z == right.origin_z &&
         left.width == right.width;
}

octaryn_client_chunk_view chunk_view_from_server_stream(
    const server_chunk_stream_file &stream) {
  octaryn_client_chunk_view view{};
  view.origin_x = stream.centerChunkX - static_cast<int32_t>(stream.radius);
  view.origin_z = stream.centerChunkZ - static_cast<int32_t>(stream.radius);
  view.width = static_cast<int32_t>(stream.radius * 2u + 1u);
  return view;
}

uint64_t hash_world_block_records(const std::vector<world_block_record> &records) {
  std::vector<world_block_record> ordered = records;
  std::sort(ordered.begin(), ordered.end(),
            [](const world_block_record &left,
               const world_block_record &right) {
              if (left.x != right.x) {
                return left.x < right.x;
              }
              if (left.y != right.y) {
                return left.y < right.y;
              }
              if (left.z != right.z) {
                return left.z < right.z;
              }
              return left.block < right.block;
            });

  uint64_t hash = 1469598103934665603ull;
  auto append = [&hash](uint64_t value) {
    for (uint32_t byte = 0u; byte < 8u; ++byte) {
      hash ^= (value >> (byte * 8u)) & 0xffu;
      hash *= 1099511628211ull;
    }
  };

  append(static_cast<uint64_t>(ordered.size()));
  for (const world_block_record &record : ordered) {
    append(static_cast<uint32_t>(record.x));
    append(static_cast<uint32_t>(record.y));
    append(static_cast<uint32_t>(record.z));
    append(record.block);
  }
  return hash;
}

void build_native_empty_world_mesh_frame(
    const octaryn_client_chunk_view &chunk_view,
    const octaryn_client_chunk_view &previous_chunk_view,
    const block_lookup &overrides,
    world_mesh_upload_frame &mesh_frame) {
  mesh_frame = {};
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!native_empty_world_chunk_range(chunk_view, min_chunk_x, max_chunk_x,
                                      min_chunk_z, max_chunk_z)) {
    if (octaryn_client_app::g_log != nullptr) {
      const size_t previous_count =
          native_empty_world_chunk_count(previous_chunk_view);
      std::fprintf(octaryn_client_app::g_log,
                   "native_empty_chunk_stream active=1 loaded=0 "
                   "preserved=0 unloaded=%zu reason=outside_bounds "
                   "render_distance=%d source=client_native_unbounded\n",
                   previous_count, chunk_view.width / 2);
      std::fflush(octaryn_client_app::g_log);
    }
    return;
  }

  const size_t chunk_count =
      static_cast<size_t>(max_chunk_x - min_chunk_x) *
      static_cast<size_t>(max_chunk_z - min_chunk_z);
  mesh_frame.chunks.reserve(chunk_count);
  mesh_frame.opaque_faces.reserve(chunk_count);

  for (int32_t chunk_z = min_chunk_z; chunk_z < max_chunk_z; ++chunk_z) {
    for (int32_t chunk_x = min_chunk_x; chunk_x < max_chunk_x; ++chunk_x) {
      for (int32_t chunk_y = kNativeEmptyWorldMinChunkY;
           chunk_y <= kNativeEmptyWorldChunkY; ++chunk_y) {
        std::vector<uint64_t> volume_faces;
        if (chunk_y == kNativeEmptyWorldMinChunkY) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 5u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_x == min_chunk_x) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 3u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_x == max_chunk_x - 1) {
          volume_faces.push_back(pack_native_empty_block_face(
              kNativeEmptyWorldChunkSize - 1u, 0u, 0u, 2u,
              kNativeEmptyWorldChunkSize, kNativeEmptyWorldChunkSize));
        }
        if (chunk_z == min_chunk_z) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, 0u, 1u, kNativeEmptyWorldChunkSize,
              kNativeEmptyWorldChunkSize));
        }
        if (chunk_z == max_chunk_z - 1) {
          volume_faces.push_back(pack_native_empty_block_face(
              0u, 0u, kNativeEmptyWorldChunkSize - 1u, 0u,
              kNativeEmptyWorldChunkSize, kNativeEmptyWorldChunkSize));
        }
        append_native_empty_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                        volume_faces);
      }

      std::array<bool, kNativeEmptyWorldChunkSize * kNativeEmptyWorldChunkSize>
          hidden_top{};
      bool has_surface_override = false;
      for (const auto &entry : overrides) {
        const block_position_key &key = entry.first;
        if (key.y != -1 ||
            floor_div_int32(key.x, kNativeEmptyWorldChunkSize) != chunk_x ||
            floor_div_int32(key.z, kNativeEmptyWorldChunkSize) != chunk_z) {
          continue;
        }

        const int32_t local_x = floor_mod_int32(key.x, kNativeEmptyWorldChunkSize);
        const int32_t local_z = floor_mod_int32(key.z, kNativeEmptyWorldChunkSize);
        hidden_top[static_cast<size_t>(local_z * kNativeEmptyWorldChunkSize +
                                       local_x)] = entry.second == 0u;
        has_surface_override = true;
      }

      std::vector<uint64_t> faces;
      if (!has_surface_override) {
        faces.push_back(pack_native_empty_block_face(
            0u, kNativeEmptyWorldLocalY, 0u, 4u, kNativeEmptyWorldChunkSize,
            kNativeEmptyWorldChunkSize));
      } else {
        faces.reserve(kNativeEmptyWorldChunkSize * kNativeEmptyWorldChunkSize);
        for (uint32_t local_z = 0u; local_z < kNativeEmptyWorldChunkSize;
             ++local_z) {
          for (uint32_t local_x = 0u; local_x < kNativeEmptyWorldChunkSize;
               ++local_x) {
            if (hidden_top[static_cast<size_t>(
                    local_z * kNativeEmptyWorldChunkSize + local_x)]) {
              continue;
            }

            faces.push_back(pack_native_empty_block_face(
                local_x, kNativeEmptyWorldLocalY, local_z, 4u, 1u, 1u));
          }
        }
      }

      append_native_empty_chunk_faces(mesh_frame, chunk_x,
                                      kNativeEmptyWorldChunkY, chunk_z, faces);
    }
  }

  for (const auto &entry : overrides) {
    const block_position_key &key = entry.first;
    if (entry.second == 0u &&
        native_empty_generated_block(key) != 0u) {
      append_native_empty_exposed_air_faces(mesh_frame, overrides, key);
      continue;
    }

    if (entry.second == 0u || key.y < 0 ||
        key.y >= kNativeEmptyWorldMaxYExclusive) {
      continue;
    }

    const int32_t chunk_x = floor_div_int32(key.x, kNativeEmptyWorldChunkSize);
    const int32_t chunk_y = floor_div_int32(key.y, kNativeEmptyWorldChunkSize);
    const int32_t chunk_z = floor_div_int32(key.z, kNativeEmptyWorldChunkSize);
    if (chunk_x < min_chunk_x || chunk_x >= max_chunk_x ||
        chunk_z < min_chunk_z || chunk_z >= max_chunk_z) {
      continue;
    }

    std::vector<uint64_t> faces;
    append_native_empty_cube_faces(
        faces,
        static_cast<uint32_t>(floor_mod_int32(key.x, kNativeEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.y, kNativeEmptyWorldChunkSize)),
        static_cast<uint32_t>(floor_mod_int32(key.z, kNativeEmptyWorldChunkSize)));
    append_native_empty_chunk_faces(mesh_frame, chunk_x, chunk_y, chunk_z,
                                    faces);
  }

  if (octaryn_client_app::g_log != nullptr) {
    const size_t previous_count =
        native_empty_world_chunk_count(previous_chunk_view);
    const size_t preserved =
        native_empty_world_chunk_overlap(previous_chunk_view, chunk_view);
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

void build_native_empty_world_mesh_frame_from_stream(
    const server_chunk_stream_file &stream, const block_lookup &overrides,
    const octaryn_client_chunk_view &previous_chunk_view,
    world_mesh_upload_frame &mesh_frame) {
  const octaryn_client_chunk_view stream_view =
      chunk_view_from_server_stream(stream);
  build_native_empty_world_mesh_frame(stream_view, previous_chunk_view,
                                      overrides, mesh_frame);

  if (octaryn_client_app::g_log != nullptr) {
    std::fprintf(octaryn_client_app::g_log,
                 "native_empty_chunk_stream active=1 source=server_background "
                 "epoch=%" PRIu64 " render_distance=%" PRIu32
                 " columns=%zu override_edits=%zu visible_chunks=%zu "
                 "opaque_faces=%zu\n",
                 stream.epoch, stream.radius, stream.columns.size(),
                 overrides.size(), mesh_frame.chunks.size(),
                 mesh_frame.opaque_faces.size());
    std::fflush(octaryn_client_app::g_log);
  }
}
