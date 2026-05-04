#include "View.h"

#include "Packing.h"

#include <algorithm>

using octaryn_client_app::server_chunk_stream_file;
using octaryn_client_app::world_block_record;

bool empty_world_chunk_range(const chunk_view &view,
                             int32_t &min_chunk_x, int32_t &max_chunk_x,
                             int32_t &min_chunk_z, int32_t &max_chunk_z) {
  min_chunk_x = view.origin_x;
  max_chunk_x = view.origin_x + view.width;
  min_chunk_z = view.origin_z;
  max_chunk_z = view.origin_z + view.width;
  return min_chunk_x < max_chunk_x && min_chunk_z < max_chunk_z;
}

size_t empty_world_chunk_count(const chunk_view &view) {
  int32_t min_chunk_x = 0;
  int32_t max_chunk_x = 0;
  int32_t min_chunk_z = 0;
  int32_t max_chunk_z = 0;
  if (!empty_world_chunk_range(view, min_chunk_x, max_chunk_x,
                               min_chunk_z, max_chunk_z)) {
    return 0u;
  }
  return static_cast<size_t>(max_chunk_x - min_chunk_x) *
         static_cast<size_t>(max_chunk_z - min_chunk_z);
}

size_t empty_world_chunk_overlap(const chunk_view &left,
                                 const chunk_view &right) {
  int32_t left_min_x = 0;
  int32_t left_max_x = 0;
  int32_t left_min_z = 0;
  int32_t left_max_z = 0;
  int32_t right_min_x = 0;
  int32_t right_max_x = 0;
  int32_t right_min_z = 0;
  int32_t right_max_z = 0;
  if (!empty_world_chunk_range(left, left_min_x, left_max_x, left_min_z,
                               left_max_z) ||
      !empty_world_chunk_range(right, right_min_x, right_max_x, right_min_z,
                               right_max_z)) {
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

bool same_chunk_view(const chunk_view &left,
                     const chunk_view &right) {
  return left.origin_x == right.origin_x && left.origin_z == right.origin_z &&
         left.width == right.width;
}

chunk_view
chunk_view_from_server_stream(const server_chunk_stream_file &stream) {
  chunk_view view{};
  view.origin_x = stream.centerChunkX - static_cast<int32_t>(stream.radius);
  view.origin_z = stream.centerChunkZ - static_cast<int32_t>(stream.radius);
  view.width = static_cast<int32_t>(stream.radius * 2u + 1u);
  return view;
}

uint64_t
hash_world_block_records(const std::vector<world_block_record> &records) {
  std::vector<world_block_record> ordered = records;
  std::sort(
      ordered.begin(), ordered.end(),
      [](const world_block_record &left, const world_block_record &right) {
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
