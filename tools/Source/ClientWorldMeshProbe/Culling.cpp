#include "Probe.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace mesh_probe {
namespace {
struct View {float x,y,z,yaw,pitch;};
constexpr View views[]{
  {0,8,0,0,-.4f},{0,8,0,.5f,-.3f},{0,8,0,1.5707963f,-.55f},
  {0,8,0,3.14159265f,-.3f},{0,8,0,-1.2f,-.1f},
  {-70,-20,0,1.5707963f,.15f},{70,-20,0,-1.5707963f,.15f},
  {0,-70,0,.35f,1.2f},{0,80,0,1.3f,-1.2f},
  {0,10,90,.1f,-.4f},{0,12,-90,3.34159265f,-.4f}};
struct Projection {int width,height;float fov;};
constexpr Projection projections[]{{128,128,1.05f},{128,64,1.5707963f},{64,128,.48995733f}};
}
void culling_cases(Fixture& f) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();
  constexpr int center_x=-4,center_z=5;
  for(int dz=-2;dz<=2;++dz)for(int dx=-2;dx<=2;++dx) {
    auto source=column(center_x+dx,center_z+dz,-48+(dx+2)*8,16);
    const auto material=static_cast<std::uint16_t>((dx+dz)%2?1:5);
    for(int z=2;z<30;++z)for(int y=0;y<16;++y)for(int x=2;x<30;++x)put(source,x,y,z,material);
    const auto mesh=f.mesh(source);
    // The coordinate map and vertical bounds are the actual source metadata.
    // Do not use Fixture::render's single-column convenience key for this test.
    r.columns.emplace(std::make_pair(source.x,source.z),mesh.gpu);
  }
  unsigned tested=0,meaningful=0,reduced=0;
  for(const auto& projection:projections)for(const auto& view:views) {
    r.width=projection.width;r.height=projection.height;
    const WorldCamera camera{float(center_x*32+16)+view.x,view.y,float(center_z*32+16)+view.z,view.yaw,view.pitch,projection.fov};
    r.culling_enabled=false;
    const auto reference=f.render(r.columns.begin()->second,camera,true,true,true);
    const auto uncull_count=r.drawn_columns;
    require(uncull_count==25,"unculled signed fixture lost a retained column");
    r.culling_enabled=true;
    const auto actual=f.render(r.columns.begin()->second,camera,true,true,true);
    unsigned covered=0;
    for(std::size_t pixel=0;pixel<reference.depth.size();++pixel)if(reference.depth[pixel]<1)++covered;
    if(covered>32)++meaningful;if(r.drawn_columns<uncull_count)++reduced;
    bool equal=std::memcmp(actual.depth.data(),reference.depth.data(),actual.depth.size()*sizeof(float))==0;
    for(unsigned target=0;target<4;++target)equal&=actual.mrt[target]==reference.mrt[target];
    std::printf("world_mesh_culling view=%u size=%dx%d fov=%g yaw=%g pitch=%g columns=%u/%u covered=%u mrt_depth=%s\n",
        tested,projection.width,projection.height,projection.fov,view.yaw,view.pitch,r.drawn_columns,uncull_count,covered,equal?"byte_identical":"different");
    if(!equal) {
      for(std::size_t pixel=0;pixel<reference.depth.size();++pixel)if(actual.depth[pixel]!=reference.depth[pixel]) {
        std::fprintf(stderr,"world_mesh_culling first_depth_difference=%zu,%zu actual=%g expected=%g\n",
            pixel%Fixture::Size,pixel/Fixture::Size,actual.depth[pixel],reference.depth[pixel]);break;
      }
    }
    require(equal,"CPU frustum culling changed production GPU pixels");++tested;
  }
  require(tested==33 && meaningful>=20 && reduced>=20,"culling camera matrix lacked meaningful visible/rejected coverage");
  r.culling_enabled=false;r.width=Fixture::Size;r.height=Fixture::Size;r.columns.clear();r.sources.clear();
  std::printf("world_mesh_culling=passed views=%u meaningful=%u reduced=%u signed_columns=25\n",tested,meaningful,reduced);
}
}
