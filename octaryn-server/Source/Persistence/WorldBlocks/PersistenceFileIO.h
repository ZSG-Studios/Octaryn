#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace octaryn::server::persistence {

inline bool read_text_file(const std::filesystem::path &path,
                           std::string &payload) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
  return file.good() || file.eof();
}

inline bool write_text_file_atomically(const std::filesystem::path &path,
                                       const std::string &payload) {
  const std::filesystem::path parent = path.parent_path();
  std::error_code error;
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      return false;
    }
  }

  const std::filesystem::path temp_path = path.string() + ".tmp";
  {
    std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      return false;
    }
    file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!file.good()) {
      std::filesystem::remove(temp_path, error);
      return false;
    }
  }

  std::filesystem::rename(temp_path, path, error);
  if (!error) {
    return true;
  }

  error.clear();
  std::filesystem::remove(path, error);
  error.clear();
  std::filesystem::rename(temp_path, path, error);
  if (error) {
    std::filesystem::remove(temp_path, error);
    return false;
  }

  return true;
}

} // namespace octaryn::server::persistence
