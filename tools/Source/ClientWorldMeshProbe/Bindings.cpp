#include "Probe.h"
#include <cmath>
namespace mesh_probe {
void binding_cases(Fixture& f) {
  f.renderer.sources.clear();auto left=column(),right=column(1,0);
  for(int z=6;z<25;++z)for(int y=2;y<13;++y)for(int x=2;x<25;++x)put(left,x,y,z,1);
  for(int z=3;z<20;++z)for(int y=3;y<22;++y)for(int x=4;x<29;++x)put(right,x,y,z,5);
  put(left,26,3,20,25);put(right,1,3,24,10);
  const auto a=f.mesh(left),b=f.mesh(right);
  f.verify("binding_left",left,a);f.verify("binding_right",right,b);
  auto joined=a.faces;joined.insert(joined.end(),b.faces.begin(),b.faces.end());
  const auto combined=f.unit_mesh(joined,0,32);
  // Rebase every patch after pass sorting; the second column's local face
  // indices must not survive concatenation into the single reference buffer.
  const auto checked_combined=f.read_mesh(combined.gpu);
  require(checked_combined.faces==combined.faces && checked_combined.patches==combined.patches,
      "combined reference changed face or patch identity");
  for(unsigned view=0;view<2;++view) {
    const float eye_z=view==0?83.f:-60.f,dy=-18,dz=13-eye_z;
    const WorldCamera camera{32,28,eye_z,std::atan2(0.f,-dz),std::atan2(dy,std::abs(dz)),1.05f};
    const auto expected=f.render(combined.gpu,camera,true,true);
    f.renderer.columns.clear();f.renderer.columns.emplace(std::make_pair(0,0),a.gpu);f.renderer.columns.emplace(std::make_pair(1,0),b.gpu);
    const auto actual=f.render(a.gpu,camera,true,true,true);
    require(f.renderer.drawn_columns==2,"binding fixture skipped a column");
    require(actual.depth==expected.depth,"column binding snapshot changed depth");
    for(unsigned target=0;target<4;++target)require(actual.mrt[target]==expected.mrt[target],"column binding snapshot changed production MRT");
    unsigned grass=0,stone=0;
    for(std::size_t i=0;i<actual.depth.size();++i)if(actual.depth[i]<1) {
      const unsigned voxel=actual.mrt[2][i*4]|(unsigned(actual.mrt[2][i*4+1])<<8),layer=(voxel>>3)&31;
      if(layer>=1 && layer<=3)++grass;if(layer==4)++stone;
    }
    require(grass>20 && stone>20,"binding proof did not show both distinct material columns");
    std::printf("world_mesh_bindings view=%u columns=2 grass_pixels=%u stone_pixels=%u mrt=byte_identical depth=identical\n",view,grass,stone);
  }
}
}
