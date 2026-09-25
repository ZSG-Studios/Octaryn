#include "WorldPersistence.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include <zlib.h>

namespace {

constexpr std::size_t GzipIoChunkSize = 64u * 1024u;

int close_gzip_file(gzFile file) {
  return gzclose(file) == Z_OK ? 0 : -2;
}

std::filesystem::path gzip_temp_path(const std::filesystem::path &path) {
  return std::filesystem::path(path.string() + ".tmp");
}

int replace_file(const std::filesystem::path &source,
                 const std::filesystem::path &target) {
  std::error_code error;
  std::filesystem::rename(source, target, error);
  if (!error) {
    return 0;
  }

  std::filesystem::remove(target, error);
  error.clear();
  std::filesystem::rename(source, target, error);
  if (!error) {
    return 0;
  }

  std::filesystem::remove(source, error);
  return -5;
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_write_gzip_file(const char *path,
                                                   const uint8_t *payload,
                                                   uint64_t payload_size) {
  if (path == nullptr || path[0] == '\0' ||
      (payload == nullptr && payload_size != 0u)) {
    return -1;
  }

  const std::filesystem::path target(path);
  const auto parent = target.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      return -2;
    }
  }

  const std::filesystem::path temp = gzip_temp_path(target);
  const std::string temp_string = temp.string();
  gzFile file = gzopen(temp_string.c_str(), "wb9");
  if (file == nullptr) {
    return -2;
  }

  uint64_t offset = 0u;
  while (offset < payload_size) {
    const uint64_t remaining = payload_size - offset;
    const auto chunk_size = static_cast<unsigned int>(
        std::min<uint64_t>(remaining, GzipIoChunkSize));
    const int written = gzwrite(file, payload + offset, chunk_size);
    if (written != static_cast<int>(chunk_size)) {
      gzclose(file);
      std::error_code error;
      std::filesystem::remove(temp, error);
      return -3;
    }
    offset += chunk_size;
  }

  const int close_result = close_gzip_file(file);
  if (close_result != 0) {
    std::error_code error;
    std::filesystem::remove(temp, error);
    return close_result;
  }

  return replace_file(temp, target);
}

int32_t octaryn_server_persistence_read_gzip_file_count(
    const char *path, uint64_t *payload_size) {
  if (path == nullptr || path[0] == '\0' || payload_size == nullptr) {
    return -1;
  }

  gzFile file = gzopen(path, "rb");
  if (file == nullptr) {
    return -2;
  }

  std::array<uint8_t, GzipIoChunkSize> buffer{};
  uint64_t total = 0u;
  int bytes = 0;
  while ((bytes = gzread(file, buffer.data(),
                         static_cast<unsigned int>(buffer.size()))) > 0) {
    total += static_cast<uint64_t>(bytes);
  }

  if (bytes < 0) {
    gzclose(file);
    return -3;
  }

  const int close_result = close_gzip_file(file);
  if (close_result != 0) {
    return close_result;
  }

  *payload_size = total;
  return 0;
}

int32_t octaryn_server_persistence_read_gzip_file_fill(
    const char *path, uint8_t *payload, uint64_t payload_capacity,
    uint64_t *payload_size) {
  if (path == nullptr || path[0] == '\0' || payload_size == nullptr ||
      (payload == nullptr && payload_capacity != 0u)) {
    return -1;
  }

  gzFile file = gzopen(path, "rb");
  if (file == nullptr) {
    return -2;
  }

  uint64_t total = 0u;
  int bytes = 0;
  while (total < payload_capacity &&
         (bytes = gzread(
              file, payload + total,
              static_cast<unsigned int>(
                  std::min<uint64_t>(payload_capacity - total,
                                     GzipIoChunkSize)))) > 0) {
    total += static_cast<uint64_t>(bytes);
  }

  if (bytes < 0) {
    gzclose(file);
    return -3;
  }

  if (total == payload_capacity) {
    std::array<uint8_t, 1> extra{};
    bytes = gzread(file, extra.data(), static_cast<unsigned int>(extra.size()));
    if (bytes > 0) {
      gzclose(file);
      return -4;
    }
    if (bytes < 0) {
      gzclose(file);
      return -3;
    }
  }

  const int close_result = close_gzip_file(file);
  if (close_result != 0) {
    return close_result;
  }

  *payload_size = total;
  return 0;
}

}
