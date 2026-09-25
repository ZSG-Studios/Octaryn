#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace octaryn::server::persistence {

struct chunk_override_block_file {
  int32_t bx = 0;
  int32_t by = 0;
  int32_t bz = 0;
  uint16_t block = 0u;
};

struct chunk_override_file {
  uint32_t version = 2u;
  int32_t cx = 0;
  int32_t cz = 0;
  std::vector<chunk_override_block_file> blocks{};
};

bool read_chunk_override_file(const std::filesystem::path &path,
                              chunk_override_file &file);

} // namespace octaryn::server::persistence
