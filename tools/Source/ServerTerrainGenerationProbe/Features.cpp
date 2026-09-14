#include "TerrainGeneration.h"
#include "TerrainDensity.h"
#include "TerrainVegetation.h"
#include <array>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace {
using namespace octaryn::basegame::terrain;
using Key=std::tuple<int32_t,int32_t,int32_t>;
constexpr OctarynServerTerrainMaterialRules rules{30,14,3,1,2,5,4,3};
unsigned checks{},trees{},seam_leaves{};
std::array<unsigned,14> species{};
void require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
void region(int32_t origin_x,int32_t origin_z) {
  std::map<Key,uint16_t> bulk;
  // Traverse halo anchors once; independent scalar queries enumerate surrounding anchors per cell.
  for(int z=-1;z<33;++z)for(int x=-1;x<33;++x) {
    unsigned logs=0,leaves=0;
    emit_vegetation(origin_x+x,origin_z+z,rules,sample_column,[&](int32_t fx,int fy,int32_t fz,uint16_t block) {
      logs+=block==LogBlock;leaves+=block==LeavesBlock;
      require(fy>=WorldMinY && fy<WorldMaxYExclusive,"emitted vegetation stays within vertical world");
      if(fx<origin_x || int64_t(fx)>=int64_t(origin_x)+32 || fz<origin_z || int64_t(fz)>=int64_t(origin_z)+32)return;
      if(sample_block(sample_column(fx,fz),fy,rules)!=AirBlock)return;
      auto& current=bulk[{fx,fy,fz}];current=merge_vegetation(current,block);
    });
    if(logs) {require((logs==4 || logs==5) && leaves==17,"authored tree has four/five logs and seventeen canopy leaves");++trees;}
  }
  for(int z=0;z<32;++z)for(int x=0;x<32;++x) {
    const int32_t wx=origin_x+x,wz=origin_z+z;
    const auto column=sample_column(wx,wz);
    for(int y=rules.water_height+1;y<=246;++y) {
      const auto base=sample_block(column,y,rules);
      const auto found=bulk.find({wx,y,wz});
      const auto expected=found==bulk.end()?base:found->second;
      uint16_t actual{};
      require(octaryn_server_terrain_generated_block(wx,y,wz,&rules,&actual)==0,"current scalar vegetation query");
      require(actual==expected,"current scalar matches independently retained bulk flora including neighboring canopies");
      if(found!=bulk.end()) {
        require(base==AirBlock,"flora cannot overwrite hillsides or water");
        if(actual<species.size())++species[actual];
        seam_leaves+=actual==LeavesBlock && (x==0 || x==31 || z==0 || z==31);
      }
    }
  }
}
void extremes() {
  for(int32_t x:{INT32_MIN,INT32_MIN+1,INT32_MAX-1,INT32_MAX})
    for(int32_t z:{INT32_MIN,INT32_MIN+1,INT32_MAX-1,INT32_MAX}) {
      emit_vegetation(x,z,rules,sample_column,[&](int32_t fx,int fy,int32_t fz,uint16_t) {
        require(std::abs(int64_t(fx)-x)<=1 && std::abs(int64_t(fz)-z)<=1 && fy>=WorldMinY && fy<WorldMaxYExclusive,
            "signed-extreme canopy emission does not overflow");
      });
      for(int y:{WorldMinY-1,WorldMaxYExclusive,INT32_MIN,INT32_MAX}) {
        uint16_t block=99;
        require(octaryn_server_terrain_generated_block(x,y,z,&rules,&block)==0 && block==AirBlock,
            "vegetated scalar preserves vertical bounds");
      }
    }
}
}
bool validate_terrain_features() {
  try {
    // Adjacent signed regions and the natural forest capture's actual world coordinates.
    for(const auto& [x,z]:std::array<std::pair<int,int>,5>{{{-32,-32},{0,0},{32,0},{288,-256},{320,-256}}})region(x,z);
    extremes();
    require(trees>0 && species[LogBlock]>0 && species[LeavesBlock]>0 && species[BushBlock]>0 && seam_leaves>0,
        "retained bulk world must contain trees bushes and seam canopy");
    for(const auto flower:FlowerBlocks)require(species[flower]>0,"every registered flower species occurs in actual world output");
    std::printf("terrain_features=passed revision=3 checks=%u trees=%u logs=%u leaves=%u bushes=%u seam_leaves=%u bulk_scalar=passed signed_bounds=passed\n",
        checks,trees,species[LogBlock],species[LeavesBlock],species[BushBlock],seam_leaves);
    return true;
  } catch(const std::exception& e) {std::fprintf(stderr,"terrain_features failed: %s\n",e.what());return false;}
}
