#pragma once
#include <cstdint>
#include <vector>

namespace octaryn::client::world_presentation {
struct StreamEdit {
  std::int32_t x{}, y{}, z{};
  std::uint16_t block{};
  bool operator==(const StreamEdit&) const = default;
};
struct ColumnOrigin {
  std::uint32_t generator_revision{3};
  std::vector<StreamEdit> edits;
  bool operator==(const ColumnOrigin&) const = default;
};
}
