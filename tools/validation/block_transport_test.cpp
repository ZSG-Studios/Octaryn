#include "BlockTransportTypes.h"
#include "block_transport_oracle.h"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace octaryn::client::rendering;
unsigned block_transport_lighting_cases();
namespace {
unsigned checks=0;
void require(bool condition,const char* message) {
  ++checks;
  if(!condition){std::fprintf(stderr,"Block transport: %s\n",message);std::exit(1);}
}
void key_cases() {
  const std::array<std::array<std::int32_t,3>,6> anchors{{
    {0,0,0},{-32,-17,-65},{31,127,63},{-16777217,-256,16777217},
    {std::numeric_limits<std::int32_t>::max()-31,0,0},
    {std::numeric_limits<std::int32_t>::min(),0,0}}};
  for(const auto& anchor:anchors)for(unsigned direction=0;direction<6;++direction) {
    const unsigned normal_axis=direction/2;
    const unsigned u_axis=normal_axis==0?2:0;
    const unsigned v_axis=normal_axis==1?2:1;
    for(unsigned v=0;v<32;++v)for(unsigned u=0;u<32;++u) {
      auto expected=anchor;
      expected[u_axis]+=static_cast<std::int32_t>(u);
      expected[v_axis]+=static_cast<std::int32_t>(v);
      std::array<float,3> local{};
      local[u_axis]=float(u)+.25f;local[v_axis]=float(v)+.75f;
      local[normal_axis]=(direction&1)?1.f:0.f;
      const auto merged=block_surface_key(anchor,direction,32,32,local);
      require(merged==BlockSurfaceKey{expected[0],expected[1],expected[2],direction},
          "merged primitive changed exact block identity");
      local[u_axis]=.25f;local[v_axis]=.75f;
      require(block_surface_key(expected,direction,1,1,local)==merged,
          "greedy-to-unit remesh changed stable identity");
      auto opposite=merged;opposite.direction^=1;
      require(!(opposite==merged),"opposite faces share identity");
    }
  }
  for(unsigned direction=0;direction<6;++direction) {
    const unsigned normal_axis=direction/2,u_axis=normal_axis==0?2:0,v_axis=normal_axis==1?2:1;
    const std::array<std::int32_t,3> anchor{-33,-17,32};
    std::array<float,3> low{-100,-100,-100},high{100,100,100};
    const auto first=block_surface_key(anchor,direction,7,13,low);
    auto last=anchor;last[u_axis]+=6;last[v_axis]+=12;
    require(first==BlockSurfaceKey{anchor[0],anchor[1],anchor[2],direction},"lower-edge clamp crossed face");
    require(block_surface_key(anchor,direction,7,13,high)==BlockSurfaceKey{last[0],last[1],last[2],direction},
        "upper-edge clamp crossed merged extent or positive normal plane");
  }
  const auto invalid=[](unsigned direction,unsigned u,unsigned v,std::array<float,3> p) {
    require(block_surface_key({0,0,0},direction,u,v,p).direction==UINT32_MAX,
        "malformed primitive accepted a cache key");
  };
  invalid(6,1,1,{});invalid(UINT32_MAX,1,1,{});invalid(0,0,1,{});invalid(0,1,0,{});
  invalid(0,33,1,{});invalid(0,1,33,{});
  for(unsigned axis=0;axis<3;++axis)for(float value:{std::numeric_limits<float>::infinity(),
      -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    std::array<float,3> position{};position[axis]=value;invalid(0,1,1,position);
  }
  require(block_surface_key({std::numeric_limits<std::int32_t>::max(),0,0},2,32,32,{31,0,0}).direction==UINT32_MAX,
      "overflowing positive block coordinate wrapped cache identity");
}
using PlantVector=std::array<float,3>;
PlantVector original_plant_normal(unsigned plane) {
  // Literal original_sprite corners for UV (0,0), (1,0), (0,1).
  const std::array<PlantVector,3> vertices=plane==0?
      std::array<PlantVector,3>{{{1,1,1},{0,1,0},{1,0,1}}}:
      std::array<PlantVector,3>{{{1,1,0},{0,1,1},{1,0,0}}};
  PlantVector normal{};
  for(unsigned axis=0;axis<3;++axis) {
    const auto a=(axis+1)%3,b=(axis+2)%3;
    normal[axis]=(vertices[1][a]-vertices[0][a])*(vertices[2][b]-vertices[0][b])-
        (vertices[1][b]-vertices[0][b])*(vertices[2][a]-vertices[0][a]);
  }
  return normal;
}
void plant_geometry_cases() {
  const std::array<std::array<std::int32_t,3>,6> anchors{{{0,0,0},{-32,-17,-65},
      {31,127,63},{-16777217,-256,16777217},{33554435,-33554433,16777219},
      {INT32_MIN,INT32_MAX,INT32_MIN+1}}};
  const float large=std::numeric_limits<float>::max();
  const std::array<PlantVector,18> views{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},
      {1,2,3},{-4,-9,1},{2,large,-3},{1e-20f,0,-2e-20f},
      {large,1,large},{-large,1,-large},{large,1,-large},{-large,1,large},
      {0,0,0},{0,1,0},{0,-large,0},{-0.f,0,-0.f},{1,0,1},{1,0,-1}}};
  for(const auto& anchor:anchors)for(unsigned layer=0;layer<29;++layer) {
    if(layer>=17 && layer<=23)continue;
    std::array<BlockSurfaceKey,4> canonical{};
    for(unsigned plane=0;plane<2;++plane) {
      const auto normal=original_plant_normal(plane);
      const unsigned submitted=plane==0?6u:8u;
      for(unsigned side=0;side<2;++side) {
        PlantVector toward=normal;if(side)for(auto& value:toward)value=-value;
        auto& key=canonical[plane*2+side];key=block_crossed_plant_key(anchor,submitted,layer,toward);
        require(key.x==anchor[0] && key.y==anchor[1] && key.z==anchor[2],
            "plant key converted exact signed anchor through world floats");
        require(key.direction==(layer*16+submitted+side) && block_transport_plant_tag(key.direction) &&
            block_transport_valid_tag(key.direction),"original plant geometry normal selected wrong side or plane");
      }
      for(const auto& toward:views) {
        double alignment=0;
        for(unsigned axis=0;axis<3;++axis)alignment+=double(normal[axis])*double(toward[axis]);
        // A finite tangent/zero vector canonically selects the first side.
        const auto expected=canonical[plane*2+(alignment<0?1:0)];
        const auto first=block_crossed_plant_key(anchor,submitted,layer,toward);
        const auto duplicate=block_crossed_plant_key(anchor,submitted+1,layer,toward);
        require(first==expected && duplicate==expected,
            "duplicate submitted plant faces changed geometric hemisphere identity");
        require(block_surface_hash(first)==block_surface_hash(duplicate),
            "duplicate plant primitives do not share a cache probe sequence");
        if(alignment!=0) {
          PlantVector opposite=toward;for(auto& value:opposite)value=-value;
          require(block_crossed_plant_key(anchor,submitted,layer,opposite)==canonical[plane*2+(alignment>0?1:0)],
              "opposite geometric hemisphere reused the front-side key");
        }
      }
    }
    for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)
      require(!(canonical[a]==canonical[b]),"crossed plant physical planes or sides alias");
  }
}
void plant_invalid_cases() {
  const auto invalid=[](unsigned direction,unsigned layer,PlantVector toward) {
    const auto key=block_crossed_plant_key({-16777217,INT32_MAX,INT32_MIN},direction,layer,toward);
    require(key.direction==UINT32_MAX && !block_transport_valid_tag(key.direction),
        "malformed plant input accepted a transport identity");
  };
  for(unsigned direction:{0u,1u,2u,3u,4u,5u,10u,11u,UINT32_MAX})invalid(direction,11,{0,0,1});
  for(unsigned layer:{17u,18u,19u,20u,21u,22u,23u,29u,30u,31u,UINT32_MAX})
    for(unsigned direction=6;direction<10;++direction)invalid(direction,layer,{0,0,1});
  for(unsigned axis=0;axis<3;++axis)for(float value:{std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
    for(unsigned direction=6;direction<10;++direction) {
      PlantVector toward{0,0,1};toward[axis]=value;invalid(direction,11,toward);
    }
  require(!block_transport_plant_tag(10) && !block_transport_valid_tag(10) &&
      !block_transport_plant_tag((11u<<4)|2u) && !block_transport_valid_tag((11u<<4)|2u),
      "unsupported plant kinds were accepted as cube identities");
}
void cube_compatibility_cases() {
  struct Golden {BlockSurfaceKey key;std::uint32_t hash;};
  // Fixed pre-plant hash vectors retain the existing cube cache ABI.
  const Golden vectors[]{{{0,0,0,0},0xa53296b4u},{{-33,-17,32,1},0x0687f30bu},
      {{INT32_MAX,INT32_MIN,-1,2},0x68be117du},{{16777217,-16777219,33554435,3},0x51b0f9dcu},
      {{-17,9,16777217,4},0x3082da6cu},{{32,127,-65,5},0x9ab25233u}};
  require(sizeof(BlockSurfaceKey)==16 && offsetof(BlockSurfaceKey,x)==0 && offsetof(BlockSurfaceKey,y)==4 &&
      offsetof(BlockSurfaceKey,z)==8 && offsetof(BlockSurfaceKey,direction)==12,"cube exact-key layout changed");
  for(const auto& vector:vectors) {
    const auto& key=vector.key;
    require(block_surface_key({key.x,key.y,key.z},key.direction,1,1,{.5f,.5f,.5f})==key,
        "plant support changed a cube unit surface identity");
    require(block_transport_valid_tag(key.direction) && !block_transport_plant_tag(key.direction),
        "plant tags replaced an existing cube direction");
    require(block_surface_hash(key)==vector.hash,"plant support changed an existing cube hash");
  }
}

void layout_cases() {
  require(sizeof(BlockTransportSurface)==64 && alignof(BlockTransportSurface)==16,"GPU surface ABI");
  require(offsetof(BlockTransportSurface,key)==0 && offsetof(BlockTransportSurface,albedo)==16 &&
      offsetof(BlockTransportSurface,state)==32 && offsetof(BlockTransportSurface,extra)==48,"GPU surface member ABI");
  require(sizeof(BlockTransportLink)==16 && offsetof(BlockTransportLink,target_epoch)==4 &&
      offsetof(BlockTransportLink,terminal)==12,"GPU link ABI");
  require(sizeof(BlockTransportCandidate)==32 && offsetof(BlockTransportCandidate,albedo)==16,"GPU admission ABI");
  require(BlockTransportCapacity>0 && (BlockTransportCapacity&(BlockTransportCapacity-1))==0,"hash capacity is power of two");
  require(BlockTransportRows<=BlockTransportCapacity && BlockTransportProbes<=BlockTransportCapacity,
      "bounded scheduler and hash probes");
  const auto links=std::uint64_t(BlockTransportCapacity)*BlockTransportLinks*sizeof(BlockTransportLink);
  const auto surface_bytes=std::uint64_t(BlockTransportCapacity)*sizeof(BlockTransportSurface);
  require(links==16*1024*1024 && surface_bytes==4*1024*1024,"production pool allocation changed planned profile");
  require(std::uint64_t(BlockTransportRows)*BlockTransportLinks<=32768,"transport rays exceed initial per-frame ceiling");
  require(std::uint64_t(BlockTransportRows)*BlockTransportDirectCalls<=4096,"direct queries exceed the per-frame ceiling");
  require(sizeof(BlockTransportWorkRow)==16 && BlockTransportSelectGroup==256,"GPU occupied-row work ABI");
}
void oracle_sensitivity() {
  // Closed-form chains establish that the GPU reference detects order and albedo errors.
  auto chain=block_transport_oracle::chain(BlockTransportLinks);
  const auto result=block_transport_oracle::evaluate(chain);
  for(unsigned channel=0;channel<3;++channel) {
    const double three=chain[1].reflectance[channel]*chain[2].reflectance[channel]*
        chain[3].reflectance[channel]*chain[3].direct[channel];
    require(std::abs(result[0].indirect[channel]-three)<1e-12,"independent three-segment path reference");
    require(result[0].outgoing[0][channel]==0 && result[0].outgoing[1][channel]==0 &&
        result[0].outgoing[2][channel]==0,"reference does not invent shorter paths");
  }
  chain.resize(2);chain[1].targets.assign(BlockTransportLinks,-1);chain[1].direct={2,3,4};
  for(unsigned i=BlockTransportLinks/2;i<BlockTransportLinks;++i)chain[0].targets[i]=-1;
  const auto half=block_transport_oracle::evaluate(chain);
  for(unsigned c=0;c<3;++c)require(std::abs(half[0].indirect[c]-.5*chain[1].reflectance[c]*chain[1].direct[c])<1e-12,
      "reference misses retain original denominator");
}
}
int main() {
  key_cases();plant_geometry_cases();plant_invalid_cases();cube_compatibility_cases();
  layout_cases();oracle_sensitivity();checks+=block_transport_lighting_cases();
  std::printf("block_transport_cpu=passed checks=%u production_keys=1 plant_geometry=1 plant_validation=1 canonical_duplicates=1 cube_hash_abi=1 production_abi=1 path_reference_sensitivity=1 lighting_temporal_policy=1 gpu_runtime=false\n",checks);
}
