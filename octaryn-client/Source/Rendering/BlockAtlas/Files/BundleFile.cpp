#include "BundleFile.h"

#include "AssetPath.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace octaryn::client::rendering {

void log_block_atlas_line(FILE *log, const char *message) {
  if (log != nullptr) {
    std::fprintf(log, "%s\n", message);
    std::fflush(log);
  }
}

bool read_block_atlas_text_file(const char *path, const char *failure_label,
                                FILE *log, std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_block_atlas_line(log, failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool block_atlas_ends_with(const std::string &value,
                           const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

bool find_first_block_atlas_bundle_file(const char *relative_directory,
                                        const char *filename_suffix,
                                        const char *failure_label, FILE *log,
                                        std::string &path) {
  char directory_buffer[4096] = {};
  if (!bundle_path_build(
          directory_buffer, sizeof(directory_buffer), relative_directory)) {
    log_block_atlas_line(log, failure_label);
    return false;
  }

  const std::filesystem::path directory(directory_buffer);
  if (!std::filesystem::exists(directory)) {
    log_block_atlas_line(log, failure_label);
    return false;
  }

  std::vector<std::filesystem::path> matches;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() &&
        block_atlas_ends_with(entry.path().filename().string(),
                              filename_suffix)) {
      matches.push_back(entry.path());
    }
  }

  if (matches.empty()) {
    log_block_atlas_line(log, failure_label);
    return false;
  }

  std::sort(matches.begin(), matches.end());
  path = matches.front().string();
  return true;
}

} // namespace octaryn::client::rendering
