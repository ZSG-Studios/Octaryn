#include "StreamSnapshot.h"
#include "TerrainDensity.h"
#include "TerrainVegetation.h"
#include <stdexcept>

namespace octaryn::client::world_presentation {
namespace {
// Existing basegame block catalog IDs; content changes require a schema revision.
struct Materials {
  int water_height{30};
  std::uint16_t water_block{14}, sand_block{3}, grass_block{1}, dirt_block{2},
      stone_block{5}, snow_block{4};
};
constexpr Materials materials{};
constexpr std::size_t index(int x, int y, int z) {
  return static_cast<std::size_t>(x + 32 * ((y - StreamWorldMinY) + StreamWorldHeight * z));
}
} // namespace

StreamColumn generate_stream_column(const SnapshotColumn& source, std::uint64_t epoch) {
  if (source.generator_revision != 3)
    throw std::invalid_argument("Unsupported terrain generator revision; expected revision 3.");
  StreamColumn result;
  result.x = source.x;
  result.z = source.z;
  result.epoch = epoch;
  result.revision = source.revision;
  result.authoritative_revision = source.authoritative_revision;
  result.blocks.resize(32 * StreamWorldHeight * 32);
  using namespace octaryn::basegame::terrain;
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      const auto sample = sample_column(source.x * 32 + x, source.z * 32 + z);
      const auto fill = classify_materials(sample, materials);
      const CaveColumnSampler caves(sample);
      for (int y = StreamWorldMinY; y < StreamWorldMinY + StreamWorldHeight; ++y) {
        result.blocks[index(x, y, z)] = sample_block_cached(caves, y, materials, fill);
      }
    }
  }
  // Complete terrain first; halo anchors reproduce neighboring canopies independently.
  for (int z = -VegetationRadius; z < 32 + VegetationRadius; ++z)
    for (int x = -VegetationRadius; x < 32 + VegetationRadius; ++x)
      emit_vegetation(source.x * 32 + x, source.z * 32 + z, materials, sample_column,
          [&](int wx, int y, int wz, std::uint16_t block) {
            const int lx = wx - source.x * 32, lz = wz - source.z * 32;
            if (lx < 0 || lx >= 32 || lz < 0 || lz >= 32) return;
            const auto cell = index(lx, y, lz);
            const std::uint16_t current = result.blocks[cell];
            // Trees may not replace solid terrain or water on an adjacent hillside.
            if (current == AirBlock || current == LogBlock || current == LeavesBlock ||
                current == BushBlock || (current >= 10 && current <= 13))
              result.blocks[cell] = merge_vegetation(current, block);
          });
  for (const auto& edit : source.edits) {
    result.blocks[index(edit.x - source.x * 32, edit.y, edit.z - source.z * 32)] = edit.block;
  }
  result.blocks.compact();
  return result;
}

} // namespace octaryn::client::world_presentation
