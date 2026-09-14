#include "TerrainFeatures.h"
#include <array>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace octaryn::basegame::terrain;
struct Event {
  int32_t x,y,z;uint16_t block;
  bool operator==(const Event&) const=default;
};
unsigned checks{};
void require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
FeatureColumn column(int32_t x=8,int32_t z=8,int32_t height=40) {
  FeatureColumn c;c.world_x=x;c.world_z=z;c.local_x=floor_mod(x,32);c.local_z=floor_mod(z,32);
  c.decoration_y=height;c.is_lowland=true;c.has_grass_surface=true;return c;
}
std::vector<Event> emit(const FeatureColumn& c,float plant,const FeatureRules& rules) {
  std::vector<Event> out;
  emit_features(c,plant*2-1,rules,[&](int32_t x,int32_t y,int32_t z,uint16_t b){out.push_back({x,y,z,b});});
  return out;
}
std::vector<Event> tree_oracle(const FeatureColumn& c,int logs,const FeatureRules& rules) {
  // Explicit canopy coordinates transcribed from recovered features.cpp's
  // callback order. The center at the top trunk is deliberately absent.
  constexpr int canopy[17][3]={{-1,0,-1},{-1,1,-1},{-1,0,0},{-1,1,0},{-1,0,1},{-1,1,1},
      {0,0,-1},{0,1,-1},{0,1,0},{0,0,1},{0,1,1},
      {1,0,-1},{1,1,-1},{1,0,0},{1,1,0},{1,0,1},{1,1,1}};
  std::vector<Event> out;
  const auto append=[&](int64_t x,int64_t y,int64_t z,uint16_t id) {
    if(x<INT32_MIN || x>INT32_MAX || z<INT32_MIN || z>INT32_MAX || y<WorldMinY || y>=WorldMaxYExclusive)return;
    out.push_back({static_cast<int32_t>(x),static_cast<int32_t>(y),static_cast<int32_t>(z),id});
  };
  for(int i=1;i<=logs;++i)append(c.world_x,int64_t(c.decoration_y)+i,c.world_z,rules.log);
  for(const auto& offset:canopy)append(int64_t(c.world_x)+offset[0],int64_t(c.decoration_y)+logs+offset[1],
      int64_t(c.world_z)+offset[2],rules.leaves);
  return out;
}
void thresholds(const FeatureRules& rules) {
  const auto c=column();
  require(emit(c,.52f,rules).empty(),"exact flower threshold emits nothing");
  require(emit(c,std::nextafter(.52f,1.f),rules)==std::vector<Event>{{8,41,8,rules.flowers[0]}},"first representable flower threshold");
  require(emit(c,.55f,rules)==std::vector<Event>{{8,41,8,rules.flowers[2]}},"exact bush threshold remains lavender");
  require(emit(c,std::nextafter(.55f,1.f),rules)==std::vector<Event>{{8,41,8,rules.bush}},"first representable bush threshold");
  require(emit(c,.8f,rules)==std::vector<Event>{{8,41,8,rules.bush}},"exact tree threshold remains bush");
  require(emit(c,std::nextafter(.8f,1.f),rules)==tree_oracle(c,4,rules),"first tree threshold preserves callback order");
  require(emit(c,1.f,rules)==tree_oracle(c,5,rules),"maximum plant noise emits five logs and seventeen leaves");
  for(unsigned i=0;i<4;++i)
    require(emit(c,(524.25f+static_cast<float>(i))/1000,rules)==std::vector<Event>{{8,41,8,rules.flowers[i]}},"original flower selection order");
  for(int local:{0,2,3,29,30,31}) {
    auto edge=c;edge.local_x=local;
    require(emit(edge,.9f,rules)==(local>2 && local<30?tree_oracle(edge,4,rules):std::vector<Event>{{8,41,8,rules.bush}}),"X anchor tree margin");
    edge=c;edge.local_z=local;
    require(emit(edge,.9f,rules)==(local>2 && local<30?tree_oracle(edge,4,rules):std::vector<Event>{{8,41,8,rules.bush}}),"Z anchor tree margin");
  }
  auto disabled=c;disabled.is_lowland=false;require(emit(disabled,.9f,rules).empty(),"nonlowland rejects vegetation");
  disabled=c;disabled.has_grass_surface=false;require(emit(disabled,.9f,rules).empty(),"nongrass rejects vegetation");
  for(float noise:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1.01f,1.01f}) {
    unsigned writes=0;emit_features(c,noise,rules,[&](auto...){++writes;});require(writes==0,"invalid noise must not emit");
  }
  for(int32_t height:{-260,-257,-256,250,254,255,INT32_MAX,INT32_MIN}) {
    auto clipped=c;clipped.decoration_y=height;
    require(emit(clipped,.9f,rules)==tree_oracle(clipped,4,rules),"signed vertical clipping preserves emission order");
  }
  for(int32_t edge:{INT32_MIN,INT32_MAX}) {
    auto clipped=c;clipped.world_x=edge;clipped.world_z=edge;
    require(emit(clipped,.9f,rules)==tree_oracle(clipped,4,rules),"extreme horizontal emission avoids overflow");
  }
}
void ordered_overlay(const FeatureRules& rules,int32_t origin,unsigned scenario) {
  using Key=std::tuple<int32_t,int32_t,int32_t>;
  std::map<Key,uint16_t> bulk;
  const auto get_column=[&](int32_t x,int32_t z) {
    require(int64_t(x)>=origin && int64_t(x)<int64_t(origin)+32 && int64_t(z)>=origin && int64_t(z)<int64_t(origin)+32,
        "scalar anchors must stay in the same signed chunk");
    const int lx=x-origin,lz=z-origin;
    const bool high=(scenario==1 && lx==9 && lz==8) || (scenario==2 && lx==8 && lz==8);
    return column(x,z,high?44:40);
  };
  const auto noise=[&](int32_t x,int32_t z) {
    const int lx=x-origin,lz=z-origin;
    const bool tree=(scenario!=2 && lx==8 && lz==8) || ((scenario==0 || scenario==2) && lx==9 && lz==8);
    return tree?.8f:-1.f; // plant .9: four logs.
  };
  const auto explicit_air=[&](int32_t x,int32_t y,int32_t z) {
    return scenario==3 && x-origin==9 && z-origin==8 && y==44;
  };
  // Recovered worldgen.cpp runs X outer/Z inner, each anchor's sparse terrain
  // writes BEFORE its flora. This order is intentionally independent of scalar lookup.
  for(int lx=0;lx<32;++lx)for(int lz=0;lz<32;++lz) {
    const int32_t x=origin+lx,z=origin+lz;const auto c=get_column(x,z);
    for(int y=38;y<=c.decoration_y;++y)bulk[{x,y,z}]=5;
    if(explicit_air(x,44,z))bulk[{x,44,z}]=AirBlock;
    emit_features(c,noise(x,z),rules,[&](int32_t fx,int32_t fy,int32_t fz,uint16_t block){bulk[{fx,fy,fz}]=block;});
  }
  for(int lx=0;lx<32;++lx)for(int lz=0;lz<32;++lz)for(int y=38;y<=46;++y) {
    const int32_t x=origin+lx,z=origin+lz;const auto c=get_column(x,z);
    const bool emitted=y<=c.decoration_y || explicit_air(x,y,z);
    const uint16_t terrain=y<=c.decoration_y?5:AirBlock;
    const auto found=bulk.find({x,y,z});const auto expected=found==bulk.end()?AirBlock:found->second;
    const auto actual=sample_feature_overlay(x,y,z,terrain,emitted,rules,get_column,noise);
    require(actual==expected,"scalar vegetation differs from ordered bulk terrain/flora overlay");
  }
  const auto at=[&](int lx,int y,int lz){auto found=bulk.find({origin+lx,y,origin+lz});return found==bulk.end()?AirBlock:found->second;};
  if(scenario==0) require(at(8,44,8)==rules.leaves && at(9,44,8)==rules.log,"later tree canopy/trunk overwrite earlier tree in original order");
  if(scenario==1) require(at(9,44,8)==5,"later sparse terrain overwrites earlier canopy");
  if(scenario==2) require(at(8,44,8)==rules.leaves,"later canopy overwrites earlier terrain");
  if(scenario==3) require(at(9,44,8)==AirBlock,"explicit air terrain write clears earlier canopy");
}
}
bool validate_terrain_features() {
  try {
    const std::array<FeatureRules,2> rules{{{6,7,9,{10,11,13,12}},{907,1203,351,{609,715,811,919}}}};
    for(const auto& ids:rules) {
      thresholds(ids);
      for(int32_t origin:{-32,0,INT32_MIN,INT32_MAX-31})for(unsigned scenario=0;scenario<4;++scenario)
        ordered_overlay(ids,origin,scenario);
    }
    std::printf("terrain_features=passed checks=%u rules=2 thresholds=passed ordered_bulk_scalar=passed signed_bounds=passed pure_feature_fixture=1\n",checks);
    return true;
  } catch(const std::exception& e) {std::fprintf(stderr,"terrain_features failed: %s\n",e.what());return false;}
}
