#pragma once
#include "TerrainColumn.h"
#include <array>
#include <cstdint>

namespace octaryn::server::world::generation {
// Geometry depends only on coordinates and the compiled generator seed/revision.
// Material rules and authoritative overrides must never be stored in this cache.
inline octaryn::basegame::terrain::TerrainColumnSample cached_column(int32_t x, int32_t z) {
  using octaryn::basegame::terrain::TerrainColumnSample;
  struct Entry { TerrainColumnSample sample{}; bool valid{}; };
  static thread_local std::array<Entry, 256> entries{};
  const auto hash = static_cast<uint32_t>(x) * 0x9e3779b1u ^
                    static_cast<uint32_t>(z) * 0x85ebca77u;
  auto &entry = entries[hash & 255u];
  if (!entry.valid || entry.sample.world_x != x || entry.sample.world_z != z) {
    entry.sample = octaryn::basegame::terrain::sample_column(x, z);
    entry.valid = true;
  }
  return entry.sample;
}
}
