#include "WorldPersistence.h"

#include "BlockStore.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <new>
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

constexpr uint32_t WorldBlockLoadSourceNone = 0u;
constexpr uint32_t WorldBlockLoadSourceAggregate = 1u;
constexpr uint32_t WorldBlockLoadSourceChunkDirectory = 2u;

struct world_block_save_tracker {
  bool dirty = false;
};

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

int32_t write_chunk_directory(
    const char *directory,
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count) {
  octaryn_server_persistence_plan_counts counts{};
  int32_t result =
      octaryn_server_persistence_plan_chunk_columns_count(edits, edit_count,
                                                          &counts);
  if (result != 0) {
    return result;
  }

  std::vector<octaryn_server_persistence_chunk_column> columns(
      counts.column_count);
  std::vector<octaryn_server_persistence_block_edit> ordered_edits(
      counts.block_count);
  octaryn_server_persistence_plan_counts written{};
  result = octaryn_server_persistence_plan_chunk_columns_fill(
      edits, edit_count, columns.data(), static_cast<uint32_t>(columns.size()),
      ordered_edits.data(), static_cast<uint32_t>(ordered_edits.size()),
      &written);
  if (result != 0) {
    return result;
  }

  uint32_t removed = 0u;
  result = octaryn_server_persistence_prune_stale_chunk_override_files(
      directory, columns.data(), static_cast<uint32_t>(columns.size()),
      &removed);
  if (result != 0) {
    return result;
  }

  return octaryn_server_persistence_write_chunk_override_directory(
      directory, columns.data(), static_cast<uint32_t>(columns.size()),
      ordered_edits.data(), static_cast<uint32_t>(ordered_edits.size()));
}

int32_t save_world_block_overrides(
    const char *aggregate_path, const char *chunk_directory,
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count) {
  if (edit_count == 0u) {
    std::error_code error;
    std::filesystem::remove(std::filesystem::path(aggregate_path), error);
    if (error) {
      return -2;
    }
    return write_chunk_directory(chunk_directory, edits, edit_count);
  }

  const octaryn_server_persistence_world_block_override_file file{
      .version = 1u,
      .block_count = edit_count,
  };
  int32_t result = octaryn_server_persistence_write_world_block_override_file(
      aggregate_path, &file, edits);
  if (result != 0) {
    return result;
  }

  return write_chunk_directory(chunk_directory, edits, edit_count);
}

int32_t load_world_block_export_edits(
    const char *aggregate_path, const char *chunk_directory,
    std::vector<octaryn_server_persistence_block_edit> &edits) {
  octaryn_server_persistence_world_block_load_source source{};
  int32_t result = octaryn_server_persistence_select_world_block_load_source(
      chunk_directory, aggregate_path, &source);
  if (result != 0) {
    return result;
  }

  if (source.source == WorldBlockLoadSourceNone || source.block_count == 0u) {
    edits.clear();
    return 0;
  }

  edits.assign(source.block_count, {});
  if (source.source == WorldBlockLoadSourceChunkDirectory) {
    uint32_t written = 0u;
    result = octaryn_server_persistence_read_chunk_override_directory_fill(
        chunk_directory, edits.data(), static_cast<uint32_t>(edits.size()),
        &written);
    if (result != 0) {
      return result;
    }
    edits.resize(written);
    return 0;
  }

  if (source.source != WorldBlockLoadSourceAggregate) {
    return -4;
  }

  octaryn_server_persistence_world_block_override_file file{};
  result = octaryn_server_persistence_read_world_block_override_file_fill(
      aggregate_path, edits.data(), static_cast<uint32_t>(edits.size()), &file);
  if (result != 0) {
    return result;
  }
  edits.resize(file.block_count);
  return 0;
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

int32_t octaryn_server_persistence_select_world_block_load_source(
    const char *chunk_directory, const char *aggregate_path,
    octaryn_server_persistence_world_block_load_source *source) {
  if (chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      source == nullptr) {
    return -1;
  }

  octaryn_server_persistence_chunk_override_directory_scan scan{};
  int32_t result = octaryn_server_persistence_scan_chunk_override_directory(
      chunk_directory, aggregate_path, &scan);
  if (result != 0) {
    return result;
  }

  if (scan.current_files_at_least_as_new_as != 0u && scan.block_count > 0u) {
    *source = octaryn_server_persistence_world_block_load_source{
        .source = WorldBlockLoadSourceChunkDirectory,
        .block_count = scan.block_count,
    };
    return 0;
  }

  octaryn_server_persistence_world_block_override_file aggregate{};
  result = octaryn_server_persistence_read_world_block_override_file_count(
      aggregate_path, &aggregate);
  if (result == 0) {
    *source = octaryn_server_persistence_world_block_load_source{
        .source = WorldBlockLoadSourceAggregate,
        .block_count = aggregate.block_count,
    };
    return 0;
  }

  *source = octaryn_server_persistence_world_block_load_source{
      .source = WorldBlockLoadSourceNone,
      .block_count = 0u,
  };
  return 0;
}

int32_t octaryn_server_persistence_read_world_block_overrides_count(
    const char *aggregate_path, const char *chunk_directory,
    uint32_t *block_count) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      block_count == nullptr) {
    return -1;
  }

  std::vector<octaryn_server_persistence_block_edit> edits;
  const int32_t result =
      load_world_block_export_edits(aggregate_path, chunk_directory, edits);
  if (result != 0) {
    return result;
  }

  *block_count = static_cast<uint32_t>(edits.size());
  return 0;
}

int32_t octaryn_server_persistence_read_world_block_overrides_fill(
    const char *aggregate_path, const char *chunk_directory,
    octaryn_server_persistence_block_edit *edits, uint32_t edit_capacity,
    uint32_t *written) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      written == nullptr) {
    return -1;
  }

  std::vector<octaryn_server_persistence_block_edit> loaded;
  const int32_t result =
      load_world_block_export_edits(aggregate_path, chunk_directory, loaded);
  if (result != 0) {
    return result;
  }
  if (edit_capacity < loaded.size()) {
    return -2;
  }
  if (!loaded.empty() && edits == nullptr) {
    return -1;
  }

  std::copy(loaded.begin(), loaded.end(), edits);
  *written = static_cast<uint32_t>(loaded.size());
  return 0;
}

int32_t octaryn_server_persistence_initialize_world_block_overrides(
    const char *aggregate_path, const char *chunk_directory,
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      (edits == nullptr && edit_count != 0u)) {
    return -1;
  }

  std::error_code error;
  if (std::filesystem::exists(std::filesystem::path(aggregate_path), error) &&
      !error) {
    return 0;
  }
  if (error) {
    return -2;
  }
  if (edit_count == 0u) {
    return 0;
  }

  return save_world_block_overrides(aggregate_path, chunk_directory, edits,
                                    edit_count);
}

int32_t octaryn_server_persistence_save_world_block_overrides(
    const char *aggregate_path, const char *chunk_directory,
    const octaryn_server_persistence_block_edit *edits, uint32_t edit_count) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      (edits == nullptr && edit_count != 0u)) {
    return -1;
  }

  return save_world_block_overrides(aggregate_path, chunk_directory, edits,
                                    edit_count);
}

int32_t octaryn_server_persistence_plan_world_block_export_columns_count(
    const char *aggregate_path, const char *chunk_directory,
    octaryn_server_persistence_plan_counts *counts) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      counts == nullptr) {
    return -1;
  }

  std::vector<octaryn_server_persistence_block_edit> edits;
  const int32_t result =
      load_world_block_export_edits(aggregate_path, chunk_directory, edits);
  if (result != 0) {
    return result;
  }

  return octaryn_server_persistence_plan_chunk_columns_count(
      edits.data(), static_cast<uint32_t>(edits.size()), counts);
}

int32_t octaryn_server_persistence_plan_world_block_export_columns_fill(
    const char *aggregate_path, const char *chunk_directory,
    octaryn_server_persistence_chunk_column *columns, uint32_t column_capacity,
    octaryn_server_persistence_block_edit *ordered_edits,
    uint32_t edit_capacity, octaryn_server_persistence_plan_counts *written) {
  if (aggregate_path == nullptr || aggregate_path[0] == '\0' ||
      chunk_directory == nullptr || chunk_directory[0] == '\0' ||
      written == nullptr) {
    return -1;
  }

  std::vector<octaryn_server_persistence_block_edit> edits;
  const int32_t result =
      load_world_block_export_edits(aggregate_path, chunk_directory, edits);
  if (result != 0) {
    return result;
  }

  return octaryn_server_persistence_plan_chunk_columns_fill(
      edits.data(), static_cast<uint32_t>(edits.size()), columns,
      column_capacity, ordered_edits, edit_capacity, written);
}

void *octaryn_server_persistence_world_block_save_tracker_create() {
  return new (std::nothrow) world_block_save_tracker{};
}

void octaryn_server_persistence_world_block_save_tracker_destroy(
    void *tracker) {
  delete static_cast<world_block_save_tracker *>(tracker);
}

void octaryn_server_persistence_world_block_save_tracker_mark_dirty(
    void *tracker) {
  if (tracker == nullptr) {
    return;
  }

  static_cast<world_block_save_tracker *>(tracker)->dirty = true;
}

uint32_t octaryn_server_persistence_world_block_save_tracker_should_save(
    const void *tracker) {
  if (tracker == nullptr) {
    return 0u;
  }

  return static_cast<const world_block_save_tracker *>(tracker)->dirty ? 1u
                                                                       : 0u;
}

void octaryn_server_persistence_world_block_save_tracker_mark_clean(
    void *tracker) {
  if (tracker == nullptr) {
    return;
  }

  static_cast<world_block_save_tracker *>(tracker)->dirty = false;
}

}
