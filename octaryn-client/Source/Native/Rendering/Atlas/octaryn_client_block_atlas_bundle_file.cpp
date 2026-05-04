#include "octaryn_client_block_atlas_bundle_file.h"

#include "octaryn_client_asset_path.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace octaryn::client::rendering {

void client_block_atlas_log_line(FILE *log, const char *message) {
  if (log != nullptr) {
    std::fprintf(log, "%s\n", message);
    std::fflush(log);
  }
}

bool client_block_atlas_read_text_file(const char *path,
                                       const char *failure_label, FILE *log,
                                       std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    client_block_atlas_log_line(log, failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool client_block_atlas_ends_with(const std::string &value,
                                  const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool client_block_atlas_find_first_bundle_file(const char *relative_directory,
                                               const char *filename_suffix,
                                               const char *failure_label,
                                               FILE *log, std::string &path) {
  char directory_buffer[4096] = {};
  if (!octaryn_client_bundle_path_build(
          directory_buffer, sizeof(directory_buffer), relative_directory)) {
    client_block_atlas_log_line(log, failure_label);
    return false;
  }

  const std::filesystem::path directory(directory_buffer);
  if (!std::filesystem::exists(directory)) {
    client_block_atlas_log_line(log, failure_label);
    return false;
  }

  std::vector<std::filesystem::path> matches;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() &&
        client_block_atlas_ends_with(entry.path().filename().string(),
                                     filename_suffix)) {
      matches.push_back(entry.path());
    }
  }

  if (matches.empty()) {
    client_block_atlas_log_line(log, failure_label);
    return false;
  }

  std::sort(matches.begin(), matches.end());
  path = matches.front().string();
  return true;
}

} // namespace octaryn::client::rendering
