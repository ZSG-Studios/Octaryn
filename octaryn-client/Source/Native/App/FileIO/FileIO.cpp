#include "FileIO.h"

#include "Log.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <iterator>

namespace octaryn_client_app {

bool read_text_file(const char *path, const char *failure_label,
                    std::string &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool read_binary_file(const char *path, const char *failure_label,
                      std::vector<uint8_t> &payload) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log_line(failure_label);
    return false;
  }

  payload.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
  return true;
}

bool write_text_file_atomic(const std::filesystem::path &path,
                            const std::string &payload,
                            const char *failure_label) {
  const std::filesystem::path parent = path.parent_path();
  std::error_code error;
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error) {
      log_line(failure_label);
      return false;
    }
  }

  std::filesystem::path temporary = path;
  temporary += ".tmp.";
  temporary += std::to_string(static_cast<unsigned long long>(SDL_GetTicksNS()));

  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      log_line(failure_label);
      return false;
    }

    file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!file.good()) {
      log_line(failure_label);
      std::filesystem::remove(temporary, error);
      return false;
    }
  }

  std::filesystem::rename(temporary, path, error);
  if (error) {
    log_line(failure_label);
    std::filesystem::remove(temporary, error);
    return false;
  }

  return true;
}

} // namespace octaryn_client_app
