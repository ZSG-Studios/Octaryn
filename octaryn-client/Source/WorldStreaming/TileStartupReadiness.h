#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
struct TileStartupReadiness {
  std::uint64_t generation{},requested_set_hash{};
  unsigned requested{},resident{},total{},visible{},visible_missing{};
  bool requested_ready{},all_manifest_ready{};
};
}
