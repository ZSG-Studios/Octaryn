#include "StreamSnapshot.h"
#include "TerrainDensity.h"

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
  StreamColumn result;
  result.x = source.x;
  result.z = source.z;
  result.epoch = epoch;
  result.revision = source.revision;
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
  for (const auto& edit : source.edits) {
    result.blocks[index(edit.x - source.x * 32, edit.y, edit.z - source.z * 32)] = edit.block;
  }
  result.blocks.compact();
  return result;
}

} // namespace octaryn::client::world_presentation
