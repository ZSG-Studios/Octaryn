#include "Probe.h"
#include <algorithm>
#include <bit>
#include <tuple>
#include <stdexcept>

namespace mesh_probe {
namespace {
constexpr int directions[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
bool is(const Block& block,const char* name) {return block.id==std::string("octaryn.basegame.block.")+name;}
bool visible(const Block& own,const Block& other,bool air) {
  if(own.fluidKind!="none") return own.fluidKind!=other.fluidKind && !other.solid;
  if(air) return true;
  if(is(own,"glass")) return !is(other,"glass") && !other.occlusion;
  if(is(own,"leaves") && is(other,"leaves")) return true;
  if(other.sprite) return true;
  return own.opaque && !other.occlusion;
}
}
unsigned pass(const Block& block) {
  if(block.sprite) return 1;
  if(block.opaque) return 0;
  if(block.fluidKind=="water") return 3;
  if(block.fluidKind=="lava") return 4;
  return 2;
}
StreamColumn column(int x,int z,int min_y,int height) {
  StreamColumn result;result.x=x;result.z=z;result.min_y=min_y;result.height=height;
  result.blocks.resize(static_cast<std::size_t>(32*32*height));return result;
}
void put(StreamColumn& c,int x,int y,int z,std::uint16_t block) {
  require(x>=0 && x<32 && z>=0 && z<32 && y>=0 && y<c.height,"fixture block out of range");
  c.blocks[static_cast<std::size_t>(x+32*(y+c.height*z))]=block;
}
std::uint16_t Fixture::sample(const StreamColumn& c,int x,int y,int z) const {
  if(y<0 || y>=c.height || x< -1 || x>32 || z< -1 || z>32) return 0;
  const int dx=x<0?-1:x>=32?1:0,dz=z<0?-1:z>=32?1:0;
  const StreamColumn* source=&c;
  if(dx || dz) {
    const auto found=renderer.sources.find({c.x+dx,c.z+dz});
    if(found==renderer.sources.end()) return 0;source=&found->second;
  }
  const int sy=c.min_y+y-source->min_y;
  if(sy<0 || sy>=source->height) return 0;
  return source->blocks[static_cast<std::size_t>((x+32)%32+32*(sy+source->height*((z+32)%32)))];
}
std::vector<Face> Fixture::expected(const StreamColumn& c) const {
  std::vector<Face> result;
  for(int z=0;z<32;++z) for(int y=0;y<c.height;++y) for(int x=0;x<32;++x) {
    const auto id=sample(c,x,y,z);if(!id) continue;
    require(id<catalog.size(),"fixture material outside catalog");const auto& material=catalog[id];
    if(is(material,"cloud")) continue;
    const auto add=[&](unsigned direction) {
      result.push_back({static_cast<unsigned>(c.x*32+x),static_cast<unsigned>(c.min_y+y),
          static_cast<unsigned>(c.z*32+z),id|(direction<<16)});
    };
    if(material.sprite) {for(unsigned d=6;d<(material.requiresSolidBase?12u:10u);++d)add(d);continue;}
    for(unsigned d=0;d<6;++d) {
      const auto other=sample(c,x+directions[d][0],y+directions[d][1],z+directions[d][2]);
      require(other<catalog.size(),"halo material outside catalog");
      if(visible(material,catalog[other],other==0)) add(d);
    }
  }
  std::sort(result.begin(),result.end());return result;
}
std::vector<Face> Fixture::verify(const char* name,const StreamColumn& c,const Mesh& mesh) const {
  const auto reference=expected(c);std::vector<Face> actual;actual.reserve(reference.size());
  unsigned expected_pass=0,left=mesh.gpu.pass_counts[0],merged=0;
  for(const auto& face:mesh.faces) {
    while(!left && expected_pass<4) left=mesh.gpu.pass_counts[++expected_pass];
    require(left!=0,"face outside declared pass ranges");--left;
    const unsigned id=face[3]&65535,d=(face[3]>>16)&15,w=((face[3]>>20)&31)+1,h=((face[3]>>25)&31)+1;
    require((face[3]>>30)==0 && id>0 && id<catalog.size(),"invalid packed rectangle ID/reserved bits");
    require(pass(catalog[id])==expected_pass,"GPU rectangle crossed pass range");
    if(expected_pass!=0) require(w==1 && h==1,"special geometry was merged");
    if(w*h>1) ++merged;
    require(d<(catalog[id].sprite?(catalog[id].requiresSolidBase?12u:10u):6u) &&
        (!catalog[id].sprite || d>=6),"invalid rectangle direction");
    const int x=std::bit_cast<int>(face[0]),y=std::bit_cast<int>(face[1]),z=std::bit_cast<int>(face[2]);
    for(unsigned v=0;v<h;++v) for(unsigned u=0;u<w;++u) {
      const int px=x+static_cast<int>(d>=2?u:0),py=y+static_cast<int>(d<2 || d>=4?v:0),
          pz=z+static_cast<int>(d<2?u:d<4?v:0);
      require(px>=c.x*32 && px<c.x*32+32 && pz>=c.z*32 && pz<c.z*32+32 &&
          py>=c.min_y && py<c.min_y+c.height,"rectangle extends outside source column");
      actual.push_back({static_cast<unsigned>(px),static_cast<unsigned>(py),static_cast<unsigned>(pz),id|(d<<16)});
      require(actual.size()<=c.blocks.size()*6,"expanded GPU surface exceeds voxel bound");
    }
  }
  std::sort(actual.begin(),actual.end());
  if(actual!=reference) {
    std::fprintf(stderr,"world_mesh_surface case=%s expected=%zu actual=%zu\n",name,reference.size(),actual.size());
    throw std::runtime_error("GPU rectangle expansion differs from independent unit-face multiset");
  }
  require(std::adjacent_find(actual.begin(),actual.end())==actual.end(),"duplicate GPU unit face");
  verify_fluids(c,mesh);
  std::printf("world_mesh_surface case=%s unit_faces=%zu gpu_quads=%zu merged_quads=%u parity=exact\n",name,actual.size(),mesh.faces.size(),merged);
  return reference;
}
}
