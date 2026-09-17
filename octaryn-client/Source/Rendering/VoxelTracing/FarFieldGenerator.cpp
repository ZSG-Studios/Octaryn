#include "FarFieldGenerator.h"
#include "TerrainDensity.h"
#include "TerrainVegetation.h"
#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

namespace octaryn::client::rendering {
namespace {
using namespace octaryn::basegame::terrain;
// Same catalog/rules as GenerateColumn.cpp, restricted to generator revision 3.
struct Rules {
  int water_height{30};
  std::uint16_t water_block{14},sand_block{3},grass_block{1},dirt_block{2},stone_block{5},snow_block{4};
};
constexpr Rules rules{};
constexpr auto All=~std::uint64_t{};
FarFieldNode uniform(std::uint16_t material,FarFieldMaterialFeatures features,void* context) {
  FarFieldNode node;node.known=node.uniform=All;node.occupied=material?All:0;node.material.fill(material);
  if(material && features)node.material_features=features(material,context);
  return node;
}
}
FarFieldBuild far_field_generate(FarFieldKey key,const FarFieldAuthority& authority,unsigned revision,
    FarFieldWork& work,FarFieldMaterialFeatures features,void* context) {
  if(revision!=3 || key.level>2)return {};
  const auto width=std::int64_t(far_field_width(key.level));
  const std::int64_t x0=std::int64_t(key.x)*width,y0=std::int64_t(key.y)*width,z0=std::int64_t(key.z)*width;
  constexpr auto low=std::numeric_limits<std::int32_t>::min(),high=std::numeric_limits<std::int32_t>::max();
  if(x0<low || x0+width-1>high || z0<low || z0+width-1>high)return {};
  if(y0>=WorldMaxYExclusive || y0+width<=WorldMinY)
    return {FarFieldBuildState::Ready,uniform(0,features,context)};
  if(!authority.complete || authority.key!=key)return {};
  if(work.edit_records<authority.edits.size())return {FarFieldBuildState::Deferred,{}};
  work.edit_records-=authority.edits.size();
  std::map<std::tuple<int,int,int>,std::uint16_t> edits;
  for(const auto& edit:authority.edits) {
    if(edit.x<x0 || edit.x>=x0+width || edit.y<y0 || edit.y>=y0+width ||
        edit.z<z0 || edit.z>=z0+width || edit.y<WorldMinY || edit.y>=WorldMaxYExclusive)return {};
    edits[{edit.x,edit.y,edit.z}]=edit.material;
  }
  const int xmin=int(std::max<std::int64_t>(low,x0-VegetationRadius));
  const int zmin=int(std::max<std::int64_t>(low,z0-VegetationRadius));
  const auto xmax=std::min<std::int64_t>(high,x0+width-1+VegetationRadius);
  const auto zmax=std::min<std::int64_t>(high,z0+width-1+VegetationRadius);
  const auto nx=std::size_t(xmax-xmin+1),nz=std::size_t(zmax-zmin+1);
  std::vector<TerrainColumnSample> columns;
  int top=246; // Terrain <=240; tree trunk <=5 plus one canopy layer.
  if(y0<=top) {
    if(work.column_samples<nx*nz || (!key.level && work.voxel_samples<64))return {FarFieldBuildState::Deferred,{}};
    work.column_samples-=nx*nz;columns.reserve(nx*nz);top=rules.water_height-1;
    for(std::int64_t z=zmin;z<=zmax;++z)for(std::int64_t x=xmin;x<=xmax;++x) {
      columns.push_back(sample_column(int(x),int(z)));
      top=std::max(top,columns.back().terrain_height+6);
    }
  }
  const bool generated_air=y0>top;
  if(key.level && !generated_air)return {FarFieldBuildState::Refine,{}};
  auto node=uniform(0,features,context);node.authority_revision=authority.revision;
  if(!key.level && !generated_air) {
    work.voxel_samples-=64;
    const auto get_column=[&](int x,int z) {
      return columns[std::size_t(std::int64_t(x)-xmin)+nx*std::size_t(std::int64_t(z)-zmin)];
    };
    for(unsigned z=0;z<4;++z)for(unsigned x=0;x<4;++x) {
      const int wx=int(x0+x),wz=int(z0+z);
      const auto column=get_column(wx,wz);
      const auto materials=classify_materials(column,rules);
      const CaveColumnSampler caves(column);
      for(unsigned y=0;y<4;++y) {
        const auto wy=int(y0+y);const auto index=x+4*(y+4*z);
        const auto terrain=sample_block_cached(caves,wy,rules,materials);
        node.material[index]=sample_vegetation(wx,wy,wz,terrain,rules,get_column);
      }
    }
  }
  // For proved-air macros, retain occupied child witnesses without inventing a
  // filled cube. Nonuniform children must be refined before exact ray shading.
  const auto child_width=width/4;
  for(const auto& [position,material]:edits) {
    const auto [x,y,z]=position;
    const auto index=unsigned((x-x0)/child_width+4*((y-y0)/child_width+4*((z-z0)/child_width)));
    if(!key.level)node.material[index]=material;
    else if(material) {
      node.occupied|=std::uint64_t{1}<<index;node.uniform&=~(std::uint64_t{1}<<index);
      node.material[index]=material;
      if(features)node.material_features|=features(material,context);
    }
  }
  if(!key.level)for(unsigned i=0;i<64;++i)if(node.material[i]) {
    node.occupied|=std::uint64_t{1}<<i;
    if(features)node.material_features|=features(node.material[i],context);
  }
  return {FarFieldBuildState::Ready,std::move(node)};
}
} // namespace octaryn::client::rendering
