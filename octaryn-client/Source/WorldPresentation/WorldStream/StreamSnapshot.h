#pragma once

#include "WorldStream.h"

#include <vector>

namespace octaryn::client::world_presentation {

struct StreamEdit {
  std::int32_t x{}, y{}, z{};
  std::uint16_t block{};
};
struct SnapshotColumn {
  std::int32_t x{}, z{};
  std::uint64_t revision{};
  std::vector<StreamEdit> edits;
  std::uint32_t generator_revision{3};
};
struct StreamSnapshot {
  std::uint64_t epoch{}, seed{};
  std::vector<SnapshotColumn> columns;
};

bool read_stream_snapshot(const std::filesystem::path& path,
                          StreamSnapshot& snapshot, std::string& error);
StreamColumn generate_stream_column(const SnapshotColumn& column,
                                    std::uint64_t epoch);

} // namespace octaryn::client::world_presentation
