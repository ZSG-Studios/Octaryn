#include "WorldPersistence.h"

#include "ChunkOverrideFileIO.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct chunk_column_directory_load {
  std::set<std::pair<int32_t, int32_t>> origins{};
  std::vector<std::filesystem::path> files{};
  std::vector<octaryn_server_persistence_block_edit> edits{};
};

bool parse_chunk_column_filename(const std::filesystem::path &path,
                                 int32_t &origin_x, int32_t &origin_z) {
  const std::string name = path.stem().string();
  constexpr std::string_view Prefix = "chunk_";
  if (!std::string_view(name).starts_with(Prefix)) {
    return false;
  }

  const std::string_view coordinates(name.data() + Prefix.size(),
                                     name.size() - Prefix.size());
  const size_t separator = coordinates.find('_');
  if (separator == std::string_view::npos) {
    return false;
  }

  const std::string_view x_text = coordinates.substr(0u, separator);
  const std::string_view z_text = coordinates.substr(separator + 1u);
  if (x_text.empty() || z_text.empty()) {
    return false;
  }

  int32_t parsed_x = 0;
  int32_t parsed_z = 0;
  const auto *x_begin = x_text.data();
  const auto *x_end = x_text.data() + x_text.size();
  const auto *z_begin = z_text.data();
  const auto *z_end = z_text.data() + z_text.size();
  const auto x_result = std::from_chars(x_begin, x_end, parsed_x);
  const auto z_result = std::from_chars(z_begin, z_end, parsed_z);
  if (x_result.ec != std::errc{} || x_result.ptr != x_end ||
      z_result.ec != std::errc{} || z_result.ptr != z_end) {
    return false;
  }

  origin_x = parsed_x;
  origin_z = parsed_z;
  return true;
}

chunk_column_directory_load
load_valid_chunk_columns(const std::filesystem::path &root) {
  chunk_column_directory_load result{};
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) {
    return result;
  }

  for (const auto &entry : std::filesystem::directory_iterator(root, error)) {
    if (error) {
      break;
    }

    std::error_code entry_error;
    if (!entry.is_regular_file(entry_error) || entry_error) {
      continue;
    }

    int32_t origin_x = 0;
    int32_t origin_z = 0;
    if (!parse_chunk_column_filename(entry.path(), origin_x, origin_z)) {
      continue;
    }

    octaryn::server::persistence::chunk_override_file file{};
    if (!octaryn::server::persistence::read_chunk_override_file(entry.path(),
                                                                file) ||
        file.cx != origin_x || file.cz != origin_z) {
      continue;
    }

    result.origins.emplace(origin_x, origin_z);
    result.files.push_back(entry.path());
    for (const auto &block : file.blocks) {
      result.edits.push_back(octaryn_server_persistence_block_edit{
          .position = {.x = block.bx, .y = block.by, .z = block.bz},
          .block = block.block,
      });
    }
  }

  std::sort(result.edits.begin(), result.edits.end(),
            [](const auto &lhs, const auto &rhs) {
              if (lhs.position.x != rhs.position.x) {
                return lhs.position.x < rhs.position.x;
              }
              if (lhs.position.y != rhs.position.y) {
                return lhs.position.y < rhs.position.y;
              }
              return lhs.position.z < rhs.position.z;
            });
  return result;
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_scan_chunk_override_directory(
    const char *directory, const char *aggregate_path,
    octaryn_server_persistence_chunk_override_directory_scan *scan) {
  if (directory == nullptr || directory[0] == '\0' || scan == nullptr) {
    return -1;
  }

  const auto loaded = load_valid_chunk_columns(std::filesystem::path(directory));
  const std::filesystem::path aggregate =
      aggregate_path == nullptr ? std::filesystem::path{} : aggregate_path;
  std::error_code error;
  const bool aggregate_exists =
      !aggregate.empty() && std::filesystem::exists(aggregate, error) && !error;
  error.clear();
  const auto aggregate_time =
      aggregate_exists ? std::filesystem::last_write_time(aggregate, error)
                       : std::filesystem::file_time_type::min();
  if (error) {
    return -2;
  }

  bool has_current_file = false;
  if (!aggregate_exists && !loaded.origins.empty()) {
    has_current_file = true;
  } else if (aggregate_exists) {
    for (const auto &path : loaded.files) {
      std::error_code entry_error;
      const auto entry_time = std::filesystem::last_write_time(path, entry_error);
      if (!entry_error && entry_time >= aggregate_time) {
        has_current_file = true;
        break;
      }
    }
  }

  *scan = octaryn_server_persistence_chunk_override_directory_scan{
      .current_files_at_least_as_new_as = has_current_file ? 1u : 0u,
      .file_count = static_cast<uint32_t>(loaded.origins.size()),
      .block_count = static_cast<uint32_t>(loaded.edits.size()),
  };
  return 0;
}

int32_t octaryn_server_persistence_read_chunk_override_directory_count(
    const char *directory, uint32_t *block_count) {
  if (directory == nullptr || directory[0] == '\0' || block_count == nullptr) {
    return -1;
  }

  const auto loaded = load_valid_chunk_columns(std::filesystem::path(directory));
  *block_count = static_cast<uint32_t>(loaded.edits.size());
  return 0;
}

int32_t octaryn_server_persistence_read_chunk_override_directory_fill(
    const char *directory, octaryn_server_persistence_block_edit *edits,
    uint32_t edit_capacity, uint32_t *written) {
  if (directory == nullptr || directory[0] == '\0' || written == nullptr) {
    return -1;
  }

  const auto loaded = load_valid_chunk_columns(std::filesystem::path(directory));
  if (edit_capacity < loaded.edits.size()) {
    return -3;
  }
  if (!loaded.edits.empty() && edits == nullptr) {
    return -1;
  }

  std::copy(loaded.edits.begin(), loaded.edits.end(), edits);
  *written = static_cast<uint32_t>(loaded.edits.size());
  return 0;
}
}
