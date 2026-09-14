#include "Probe.h"
#include <cmath>
#include <map>

namespace mesh_probe {
namespace {
using Columns=std::map<std::pair<int,int>,WorldColumnGpu>;
void check_floor_rays(const Image& actual,const WorldCamera& camera,unsigned view,
                      unsigned& checked_pixels,unsigned& holes) {
  const double sy=std::sin(double(camera.yaw)),cy=std::cos(double(camera.yaw));
  const double sp=std::sin(double(camera.pitch)),cp=std::cos(double(camera.pitch));
  const double tangent=std::tan(double(camera.vertical_fov)*.5);
  unsigned missing=0;
  for(unsigned y=0;y<Fixture::Size;++y)for(unsigned x=0;x<Fixture::Size;++x) {
    const double horizontal=(2*(double(x)+.5)/Fixture::Size-1)*tangent;
    const double vertical=(1-2*(double(y)+.5)/Fixture::Size)*tangent;
    // Pixel-center perspective ray, expressed directly in the camera basis.
    // Its parameter is view depth because the forward coefficient is one.
    const double dx=sy*cp+horizontal*cy-vertical*sy*sp;
    const double dy=sp+vertical*cp;
    const double dz=-cy*cp+horizontal*sy+vertical*cy*sp;
    if(dy>=0)continue;
    // y=5 lies inside the common solid slab [0,6]. A ray reaching this
    // interior must first hit its top/side, or one of the higher solid steps.
    const double depth=(5-double(camera.y))/dy;
    if(depth<=.1 || depth>=8192)continue;
    const double wx=double(camera.x)+depth*dx,wz=double(camera.z)+depth*dz;
    if(wx<=-63 || wx>=31 || wz<=-31 || wz>=63)continue;
    ++checked_pixels;
    const auto value=actual.depth[y*Fixture::Size+x];
    if(!std::isfinite(value) || value>=1) {
      if(missing<8)std::fprintf(stderr,"world_mesh_analytic_hole view=%u pixel=%u,%u floor_xz=%.9g,%.9g depth=%.9g\n",
          view,x,y,wx,wz,value);
      ++missing;
    }
  }
  holes+=missing;
}
void compare_interior(const Image& reference,const Image& actual,unsigned view,
                      unsigned& checked_pixels,unsigned& holes) {
  unsigned interior=0,missing=0;
  for(unsigned y=1;y+1<Fixture::Size;++y)for(unsigned x=1;x+1<Fixture::Size;++x) {
    const auto pixel=y*Fixture::Size+x;
    bool inside=true;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
      const auto neighbor=static_cast<unsigned>(int(y)+dy)*Fixture::Size+static_cast<unsigned>(int(x)+dx);
      inside&=std::isfinite(reference.depth[neighbor]) && reference.depth[neighbor]<1;
    }
    if(!inside)continue;
    ++interior;
    require(std::isfinite(actual.depth[pixel]),"nonfinite seam depth");
    if(actual.depth[pixel]>=1) {
      if(missing<8)std::fprintf(stderr,"world_mesh_seam_hole view=%u pixel=%u,%u reference_depth=%.9g actual_depth=%.9g\n",
          view,x,y,reference.depth[pixel],actual.depth[pixel]);
      ++missing;
    }
  }
  checked_pixels+=interior;holes+=missing;
  std::printf("world_mesh_seam view=%u interior=%u uncovered=%u silhouette_allowance=0\n",view,interior,missing);
}
}
void seam_cases(Fixture& f) {
  auto& r=f.renderer;r.columns.clear();r.sources.clear();r.culling_enabled=false;
  r.width=Fixture::Size;r.height=Fixture::Size;
  unsigned stone=0,grass=0;
  for(unsigned i=1;i<f.catalog.size();++i) {
    if(f.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
    if(f.catalog[i].id=="octaryn.basegame.block.grass")grass=i;
  }
  require(stone && grass,"seam fixture catalog materials missing");
  // Neighbor payloads precede every mesh: the oracle sees the same complete halo.
  for(int cz=-1;cz<=1;++cz)for(int cx=-2;cx<=0;++cx) {
    auto source=column(cx,cz,0,16);
    for(int z=0;z<32;++z)for(int x=0;x<32;++x) {
      const int wx=cx*32+x,wz=cz*32+z;
      const int top=6+(wx>=-8 && wz>=20?4:0)+(wz>=40?2:0);
      // Alternating partition lengths force long edges to meet many short edges;
      // both materials are solid, so this changes partitioning without holes.
      const auto material=static_cast<std::uint16_t>(cx==-1 && ((x/3+z/5)%3==0)?grass:stone);
      for(int y=0;y<top;++y)put(source,x,y,z,material);
    }
    r.sources.emplace(std::make_pair(cx,cz),std::move(source));
  }
  Columns merged,units;unsigned merged_faces=0,unit_faces=0;
  for(const auto& [coordinate,source]:r.sources) {
    const auto gpu=f.mesh(source);
    const auto expanded=f.verify("seam_patch_column",source,gpu);
    const auto unit=f.unit_mesh(expanded,source.min_y,source.height);
    merged.emplace(coordinate,gpu.gpu);units.emplace(coordinate,unit.gpu);
    merged_faces+=gpu.gpu.face_count;unit_faces+=unit.gpu.face_count;
  }
  require(merged_faces<unit_faces/2,"seam fixture did not create substantial greedy edges");
  unsigned tested=0,interior=0,holes=0,analytic_pixels=0,analytic_holes=0;
  for(unsigned side=0;side<4;++side)for(unsigned phase=0;phase<64;++phase) {
    const float angle=float(side)*1.57079632679f+(float(phase)-31.5f)*.000875f;
    const float distance=72+float(phase)*.015625f;
    const float x=-16+std::sin(angle)*distance,z=16+std::cos(angle)*distance;
    const float y=12.0625f+float(phase)*.01171875f;
    const float dx=-16-x,dz=16-z,dy=6-y;
    const WorldCamera camera{x,y,z,std::atan2(dx,-dz),std::atan2(dy,std::hypot(dx,dz)),.72f};
    r.columns=units;const auto expected=f.render(r.columns.begin()->second,camera,false,false,true);
    r.columns=merged;const auto actual=f.render(r.columns.begin()->second,camera,false,false,true);
    require(r.drawn_columns==9,"seam fixture omitted a retained column");
    check_floor_rays(actual,camera,tested,analytic_pixels,analytic_holes);
    compare_interior(expected,actual,tested++,interior,holes);
  }
  require(tested==256 && interior>4096,"seam fixture lacked moving oblique interior coverage");
  require(holes==0,"greedy raster exposed background inside the unit-mesh surface");
  std::printf("world_mesh_seams_analytic pixels=%u missing=%u plane_y=5 reference_mesh=0\n",analytic_pixels,analytic_holes);
  require(analytic_pixels>4096,"analytic seam oracle lacked meaningful floor coverage");
  require(analytic_holes==0,"greedy raster exposed background on an analytically covered floor ray");
  require(r.debug.errors.load()==0,"seam fixture validation errors");
  std::printf("world_mesh_seams=passed views=%u interior_pixels=%u merged_quads=%u unit_quads=%u inflation=0\n",
      tested,interior,merged_faces,unit_faces);
  r.columns.clear();r.sources.clear();
}
}
