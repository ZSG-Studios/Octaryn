#include "StreamSnapshot.h"
#include "ColumnHalo.h"
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
std::shared_ptr<const ColumnOrigin> origin(const SnapshotColumn& source) {
  if(source.generator_revision!=3)
    throw std::invalid_argument("Unsupported terrain generator revision; expected revision 3.");
  return std::make_shared<const ColumnOrigin>(ColumnOrigin{source.generator_revision,source.edits});
}
} // namespace

StreamColumn generate_stream_column(const SnapshotColumn& source, std::uint64_t epoch) {
  StreamColumn result;
  result.origin=origin(source);
  result.x = source.x;
  result.z = source.z;
  result.epoch = epoch;
  result.revision = source.revision;
  result.authoritative_revision = source.authoritative_revision;
  result.blocks.resize(32 * StreamWorldHeight * 32);
  auto blocks=result.blocks.mutable_values();
  using namespace octaryn::basegame::terrain;
  static_assert(AirBlock==0);
  for (int z = 0; z < 32; ++z) {
    for (int x = 0; x < 32; ++x) {
      const auto sample = sample_column(source.x * 32 + x, source.z * 32 + z);
      const auto fill = classify_materials(sample, materials);
      const CaveColumnSampler caves(sample);
      const int top=std::min(StreamWorldMinY+StreamWorldHeight-1,
          std::max(sample.terrain_height,materials.water_height-1));
      // resize already initialized every guaranteed-air cell above this range.
      for (int y = StreamWorldMinY; y <= top; ++y) {
        blocks[index(x, y, z)] = sample_block_cached(caves, y, materials, fill);
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
            const std::uint16_t current = blocks[cell];
            // Trees may not replace solid terrain or water on an adjacent hillside.
            if (current == AirBlock || current == LogBlock || current == LeavesBlock ||
                current == BushBlock || (current >= 10 && current <= 13))
              blocks[cell] = merge_vegetation(current, block);
          });
  for (const auto& edit : source.edits) {
    blocks[index(edit.x - source.x * 32, edit.y, edit.z - source.z * 32)] = edit.block;
  }
  result.blocks.compact();
  result.generated_blocks=result.blocks;
  return result;
}

void prepare_stream_halo(StreamColumn& source,
    const std::array<std::optional<SnapshotColumn>,8>& neighbors) {
  if(source.height<=0 || source.height>StreamWorldHeight || source.min_y<StreamWorldMinY ||
      source.min_y>StreamWorldMinY+StreamWorldHeight-source.height)
    throw std::invalid_argument("Invalid generated column halo extent.");
  auto halo=std::make_shared<ColumnHalo>();
  halo->min_y=source.min_y;halo->height=source.height;
  halo->blocks.resize(132u*static_cast<std::size_t>(source.height));
  auto blocks=halo->blocks.mutable_values();
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
    if(!dx && !dz)continue;
    const auto slot=column_neighbor_index(dx,dz);
    if(!neighbors[slot])continue;
    const auto& neighbor=*neighbors[slot];
    if(neighbor.x!=source.x+dx || neighbor.z!=source.z+dz)
      throw std::invalid_argument("Invalid generated column halo neighbor.");
    halo->neighbors[slot]=origin(neighbor);
  }
  const auto included=[&](int x,int z) {
    if(x < -1 || x > 32 || z < -1 || z > 32 || (x>=0 && x<32 && z>=0 && z<32))return false;
    const int dx=x<0?-1:x>=32?1:0,dz=z<0?-1:z>=32?1:0;
    return bool(halo->neighbors[column_neighbor_index(dx,dz)]);
  };
  using namespace octaryn::basegame::terrain;
  for(int z=-1;z<=32;++z)for(int x=-1;x<=32;++x) {
    if(!included(x,z))continue;
    const auto sample=sample_column(source.x*32+x,source.z*32+z);
    const auto fill=classify_materials(sample,materials);
    const CaveColumnSampler caves(sample);
    const int top=std::min(source.min_y+source.height-1,
        std::max(sample.terrain_height,materials.water_height-1));
    const auto first=column_halo_index(x,0,z,source.height);
    for(int y=source.min_y;y<=top;++y)
      blocks[first+static_cast<std::size_t>(y-source.min_y)]=sample_block_cached(caves,y,materials,fill);
  }
  // Match full-column anchor order, expanded only enough to cover the border.
  for(int z=-1-VegetationRadius;z<=32+VegetationRadius;++z)
    for(int x=-1-VegetationRadius;x<=32+VegetationRadius;++x) {
      // Interior anchors farther than their reach cannot affect this border.
      if(x>=VegetationRadius && x<32-VegetationRadius &&
          z>=VegetationRadius && z<32-VegetationRadius)continue;
      emit_vegetation(source.x*32+x,source.z*32+z,materials,sample_column,
          [&](int wx,int y,int wz,std::uint16_t block) {
            const int lx=wx-source.x*32,lz=wz-source.z*32;
            if(!included(lx,lz) || y<source.min_y || y>=source.min_y+source.height)return;
            const auto cell=column_halo_index(lx,y-source.min_y,lz,source.height);
            const std::uint16_t current=blocks[cell];
            if(current==AirBlock || current==LogBlock || current==LeavesBlock ||
                current==BushBlock || (current>=10 && current<=13))
              blocks[cell]=merge_vegetation(current,block);
          });
    }
  // Each neighbor owns its authoritative edits, including replacements with air.
  for(const auto& neighbor:neighbors)if(neighbor)for(const auto& edit:neighbor->edits) {
    const int x=edit.x-source.x*32,z=edit.z-source.z*32;
    if(included(x,z) && edit.y>=source.min_y && edit.y<source.min_y+source.height)
      blocks[column_halo_index(x,edit.y-source.min_y,z,source.height)]=edit.block;
  }
  halo->blocks.compact();
  source.mesh_halo=std::move(halo);
}

} // namespace octaryn::client::world_presentation
