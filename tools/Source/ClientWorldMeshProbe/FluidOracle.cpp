#include "Probe.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace mesh_probe {
void Fixture::verify_fluids(const StreamColumn& c,const Mesh& mesh) const {
  const unsigned start=mesh.gpu.pass_counts[0]+mesh.gpu.pass_counts[1]+mesh.gpu.pass_counts[2];
  for(std::size_t i=0;i<mesh.fluids.size();++i) {
    const auto& face=mesh.faces[start+i];const auto& own=catalog[face[3]&65535];
    const int x=std::bit_cast<int>(face[0])-c.x*32,y=std::bit_cast<int>(face[1])-c.min_y,z=std::bit_cast<int>(face[2])-c.z*32;
    const auto block=[&](int dx,int dy,int dz)->const Block& {return catalog[sample(c,x+dx,y+dy,z+dz)];};
    const auto same=[&](const Block& other){return other.fluidKind==own.fluidKind;};
    const auto height=[&](int dx,int dy,int dz) {
      const auto& b=block(dx,dy,dz);
      if(!same(b)) return b.solid?-1.f:0.f;
      if(same(block(dx,dy+1,dz))) return 1.f;
      return static_cast<float>(std::clamp(8-b.fluidLevel,1,8))/9.f;
    };
    const auto corner=[&](int dx,int dz) {
      const float a=height(0,0,0),b=height(dx,0,0),d=height(0,0,dz);
      if(b>=1 || d>=1) return 1.f;
      float diagonal=-1;
      if(b>0 || d>0) {diagonal=height(dx,0,dz);if(diagonal>=1) return 1.f;}
      float sum=0,weights=0;
      for(const float value:{a,b,d,diagonal}) if(value>=0) {
        const float weight=value>=.8f?10.f:1.f;sum+=value*weight;weights+=weight;
      }
      return sum/weights;
    };
    std::array<float,8> expected{corner(-1,-1),corner(1,-1),corner(1,1),corner(-1,1),0,0,0,0};
    float fx=0,fz=0;
    constexpr int sides[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(const auto& side:sides) {
      const int dx=side[0],dz=side[1];const auto& b=block(dx,0,dz);float pull=0;
      if(same(b)) pull=height(0,0,0)-height(dx,0,dz);
      else if(!b.occlusion && same(block(dx,-1,dz))) pull=height(0,0,0)-(height(dx,-1,dz)-8.f/9.f);
      fx+=static_cast<float>(dx)*pull;fz+=static_cast<float>(dz)*pull;
    }
    const float magnitude=std::hypot(fx,fz);
    if(magnitude>.0001f) {expected[4]=fx/magnitude;expected[5]=fz/magnitude;expected[6]=std::clamp(std::floor(magnitude*15+.5f),1.f,15.f);}
    expected[7]=same(block(0,1,0))?1.f:0.f;
    for(unsigned value=0;value<8;++value)
      require(std::isfinite(mesh.fluids[i][value]) && std::abs(mesh.fluids[i][value]-expected[value])<=.0001f,
          "original fluid heights/flow changed during opaque greedy meshing");
  }
}
}
