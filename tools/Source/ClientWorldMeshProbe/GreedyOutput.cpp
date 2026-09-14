#include "Probe.h"
#include <algorithm>
#include <stdexcept>
namespace mesh_probe {
namespace {
std::uint16_t material(const Fixture& f,const char* name) {
  const auto wanted=std::string("octaryn.basegame.block.")+name;
  for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return static_cast<std::uint16_t>(i);
  throw std::runtime_error("greedy output fixture material missing");
}
std::array<int,3> position(unsigned direction,int u,int v) {
  const int normal=(direction&1)?31:0;
  if(direction<2)return {normal,v,u};
  if(direction<4)return {u,normal,v};
  return {u,v,normal};
}
Face rectangle(const StreamColumn& c,unsigned direction,int u,int v,unsigned width,unsigned height,std::uint16_t id) {
  const auto p=position(direction,u,v);
  return {static_cast<std::uint32_t>(c.x*32+p[0]),static_cast<std::uint32_t>(c.min_y+p[1]),
      static_cast<std::uint32_t>(c.z*32+p[2]),id|(direction<<16)|((width-1)<<20)|((height-1)<<25)};
}
void check(Fixture& f,const StreamColumn& c,unsigned direction,std::vector<Face> expected,const std::string& name) {
  f.renderer.sources.clear();const auto mesh=f.mesh(c);
  f.verify(name.c_str(),c,mesh); // Independent unit-surface and complete patch/argument oracles.
  std::vector<Face> actual;
  for(const auto& face:mesh.faces)if(((face[3]>>16)&15)==direction)actual.push_back(face);
  std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());
  require(actual==expected,"cooperative output changed the exact greedy rectangle partition");
}
}
void greedy_output_cases(Fixture& f) {
  const auto stone=material(f,"stone"),grass=material(f,"grass");
  constexpr std::array counts{1u,31u,32u,33u,63u,64u,65u,1024u};
  unsigned cases=0;
  for(unsigned direction=0;direction<6;++direction)for(const auto count:counts) {
    auto c=column(-2,3,-32,32);std::vector<Face> expected;
    for(unsigned i=0;i<count;++i) {
      const int u=static_cast<int>(i%32),v=static_cast<int>(i/32);
      const auto id=((u+v)&1)?grass:stone;const auto p=position(direction,u,v);
      put(c,p[0],p[1],p[2],id);expected.push_back(rectangle(c,direction,u,v,1,1,id));
    }
    check(f,c,direction,std::move(expected),"greedy_batch_tail_"+std::to_string(direction)+"_"+std::to_string(count));++cases;
  }
  // Every row has a different width and an unlike neighbor material. This has
  // exactly 32 rectangles with patch lengths1..32 and nonuniform prefix offsets.
  for(unsigned direction=0;direction<6;++direction) {
    auto c=column(2,-3,128,32);std::vector<Face> expected;
    for(int v=0;v<32;++v) {
      const auto id=(v&1)?grass:stone;
      for(int u=0;u<=v;++u) {const auto p=position(direction,u,v);put(c,p[0],p[1],p[2],id);}
      expected.push_back(rectangle(c,direction,0,v,static_cast<unsigned>(v+1),1,id));
    }
    check(f,c,direction,std::move(expected),"greedy_patch_prefix_"+std::to_string(direction));++cases;
  }
  // A full batch of unit faces followed by a 32x30 fan rectangle exercises the
  // second batch's face base and a62-patch tail without a shared material edge.
  for(unsigned direction=0;direction<6;++direction) {
    auto c=column(-3,-2,0,32);std::vector<Face> expected;
    for(int u=0;u<32;++u) {
      const auto id=(u&1)?grass:stone;const auto p=position(direction,u,0);
      put(c,p[0],p[1],p[2],id);expected.push_back(rectangle(c,direction,u,0,1,1,id));
    }
    for(int v=2;v<32;++v)for(int u=0;u<32;++u) {const auto p=position(direction,u,v);put(c,p[0],p[1],p[2],stone);}
    expected.push_back(rectangle(c,direction,0,2,32,30,stone));
    check(f,c,direction,std::move(expected),"greedy_fan_tail_"+std::to_string(direction));++cases;
  }
  f.renderer.sources.clear();
  std::printf("world_mesh_cooperative_output=passed cases=%u directions=6 max_plane_rectangles=1024 max_batch_rectangles=32 partition=exact\n",cases);
}
}
