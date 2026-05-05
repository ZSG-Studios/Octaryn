#include "WorldPersistence.h"

#include "BlockStore.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace {

using octaryn::server::world::blocks::ChunkDepth;
using octaryn::server::world::blocks::ChunkWidth;

using column_key = std::pair<int32_t, int32_t>;

int32_t floor_div(int32_t value, int32_t divisor) {
  const int32_t quotient = value / divisor;
  const int32_t remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

column_key column_origin_for(
    const octaryn_server_persistence_block_position &position) {
  return {floor_div(position.x, ChunkWidth) * ChunkWidth,
          floor_div(position.z, ChunkDepth) * ChunkDepth};
}

using grouped_edits =
    std::map<column_key, std::vector<octaryn_server_persistence_block_edit>>;

grouped_edits group_edits(const octaryn_server_persistence_block_edit *edits,
                          uint32_t edit_count) {
  grouped_edits grouped;
  if (edits == nullptr && edit_count != 0u) {
    return grouped;
  }

  for (uint32_t index = 0; index < edit_count; ++index) {
    grouped[column_origin_for(edits[index].position)].push_back(edits[index]);
  }

  for (auto &[origin, column_edits] : grouped) {
    (void)origin;
    std::sort(column_edits.begin(), column_edits.end(), [](const auto &left,
                                                           const auto &right) {
      if (left.position.y != right.position.y) {
        return left.position.y < right.position.y;
      }
      if (left.position.x != right.position.x) {
        return left.position.x < right.position.x;
      }
      return left.position.z < right.position.z;
    });
  }

  return grouped;
}

octaryn_server_persistence_plan_counts counts_for(const grouped_edits &grouped,
                                                  uint32_t edit_count) {
  return octaryn_server_persistence_plan_counts{
      .column_count = static_cast<uint32_t>(grouped.size()),
      .block_count = edit_count,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_plan_chunk_columns_count(
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count,
    octaryn_server_persistence_plan_counts *counts) {
  if (counts == nullptr || (edits == nullptr && edit_count != 0u)) {
    return -1;
  }

  *counts = counts_for(group_edits(edits, edit_count), edit_count);
  return 0;
}

int32_t octaryn_server_persistence_plan_chunk_columns_fill(
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count,
    octaryn_server_persistence_chunk_column *columns, uint32_t column_capacity,
    octaryn_server_persistence_block_edit *ordered_edits,
    uint32_t edit_capacity, octaryn_server_persistence_plan_counts *written) {
  if (written == nullptr || (edits == nullptr && edit_count != 0u)) {
    return -1;
  }

  const grouped_edits grouped = group_edits(edits, edit_count);
  const auto counts = counts_for(grouped, edit_count);
  *written = counts;
  if (column_capacity < counts.column_count || edit_capacity < counts.block_count) {
    return -2;
  }

  if ((counts.column_count != 0u && columns == nullptr) ||
      (counts.block_count != 0u && ordered_edits == nullptr)) {
    return -1;
  }

  uint32_t column_index = 0u;
  uint32_t block_index = 0u;
  for (const auto &[origin, column_edits] : grouped) {
    const uint32_t offset = block_index;
    for (const auto &edit : column_edits) {
      ordered_edits[block_index++] = edit;
    }

    columns[column_index++] = octaryn_server_persistence_chunk_column{
        .origin_x = origin.first,
        .origin_z = origin.second,
        .block_offset = offset,
        .block_count = static_cast<uint32_t>(column_edits.size()),
    };
  }

  return 0;
}

}
