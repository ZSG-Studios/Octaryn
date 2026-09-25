#pragma once

#include "WorldStream.h"
#include "ColumnOrigin.h"

#include <array>
#include <optional>
#include <vector>

namespace octaryn::client::world_presentation {

struct SnapshotColumn {
  std::int32_t x{}, z{};
  std::uint64_t revision{};
  std::vector<StreamEdit> edits;
  std::uint32_t generator_revision{3};
  std::uint64_t authoritative_revision{};
};
struct StreamSnapshot {
  std::uint64_t epoch{}, seed{};
  std::vector<SnapshotColumn> columns;
};

bool read_stream_snapshot(const std::filesystem::path& path,
                          StreamSnapshot& snapshot, std::string& error);
StreamColumn generate_stream_column(const SnapshotColumn& column,
                                    std::uint64_t epoch);
void prepare_stream_halo(StreamColumn& column,
    const std::array<std::optional<SnapshotColumn>,8>& neighbors);

} // namespace octaryn::client::world_presentation
