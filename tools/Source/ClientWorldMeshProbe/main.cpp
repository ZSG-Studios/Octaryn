#include "Probe.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <set>

namespace mesh_probe {
namespace {
unsigned id(const Fixture& f,const char* name) {
  const std::string wanted=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=0;i<f.catalog.size();++i) if(f.catalog[i].id==wanted) return i;
  throw std::runtime_error("fixture catalog ID missing: "+wanted);
}
void box(StreamColumn& c,int x0,int y0,int z0,int x1,int y1,int z1,unsigned material) {
  for(int z=z0;z<z1;++z)for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)put(c,x,y,z,static_cast<std::uint16_t>(material));
}
void check(Fixture& f,const char* name,const StreamColumn& c,std::size_t quads=0) {
  const auto result=f.mesh(c);f.verify(name,c,result);
  if(quads)require(result.faces.size()==quads,"known rectangular fixture did not merge exactly");
}
WorldCamera look(float x,float y,float z,float tx,float ty,float tz) {
  const float dx=tx-x,dy=ty-y,dz=tz-z,length=std::sqrt(dx*dx+dy*dy+dz*dz);
  return {x,y,z,std::atan2(dx,-dz),std::asin(dy/length),1.05f};
}
}
void surface_cases(Fixture& f) {
  const auto stone=id(f,"stone"),grass=id(f,"grass"),leaves=id(f,"leaves");
  auto c=column();check(f,"empty",c);
  put(c,15,15,15,static_cast<std::uint16_t>(stone));check(f,"isolated",c,6);
  c=column();box(c,0,0,0,32,32,32,stone);check(f,"solid32",c,6);
  f.renderer.sources.clear();c=column(-2,3,-256,512);box(c,0,0,0,32,512,32,stone);c.blocks.compact();check(f,"signed_full_height",c,66);
  f.renderer.sources.clear();c=column(-1,-1,-17,33);
  for(int z=0;z<32;++z)for(int y=0;y<33;++y)for(int x=0;x<32;++x)
    if((x+y+z)%2==0)put(c,x,y,z,static_cast<std::uint16_t>(stone));
  check(f,"air_checkerboard_partial_band",c);
  f.renderer.sources.clear();c=column();
  for(int z=0;z<32;++z)for(int y=0;y<32;++y)for(int x=0;x<32;++x)
    put(c,x,y,z,static_cast<std::uint16_t>((x+y+z)%2?stone:grass));
  check(f,"material_checkerboard",c,6144);
  c=column();
  for(int z=0;z<32;++z)for(int x=0;x<32;++x)
    if(x<3 || z<3 || (x>8 && x<24 && (z==9 || z==23 || x==9 || x==23)))put(c,x,7,z,static_cast<std::uint16_t>(stone));
  check(f,"concave_ring_tjunction",c);
  c=column(-3,-4,-32,64);box(c,0,0,0,32,64,32,stone);f.renderer.sources.clear();
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx || dz) {
    auto neighbor=column(c.x+dx,c.z+dz,-16,32);box(neighbor,0,0,0,32,32,32,stone);
    f.renderer.sources.emplace(std::make_pair(neighbor.x,neighbor.z),std::move(neighbor));
  }
  check(f,"all_signed_halo_neighbors_shifted_height",c);
  f.renderer.sources.clear();c=column(2,-3,128,32);
  box(c,1,1,1,5,2,5,leaves);put(c,10,2,2,static_cast<std::uint16_t>(id(f,"glass")));
  unsigned slot=0;
  for(unsigned material=1;material<f.catalog.size();++material) {
    const auto& block=f.catalog[material];
    if(block.sprite || block.fluidKind=="water" || block.fluidKind=="lava") {
      const int x=2+int(slot%6)*5,z=8+int(slot/6)*4;
      require(z<31,"special fixture grid capacity");put(c,x,4,z,static_cast<std::uint16_t>(material));
      if(block.fluidKind!="none")put(c,x+1,3,z,static_cast<std::uint16_t>(material));
      ++slot;
    }
  }
  check(f,"catalog_specials_and_fluid_levels",c);
}
void cutout_case(Fixture& f) {
  f.renderer.sources.clear();auto c=column();box(c,2,4,2,30,5,30,id(f,"leaves"));
  const auto gpu=f.mesh(c);const auto expanded=f.verify("raster_cutout",c,gpu);
  const auto camera=look(38,34,39,16,4,16);
  std::set<Face> remaining(expanded.begin(),expanded.end());
  require(remaining.size()==expanded.size(),"independent cutout oracle contains duplicate faces");
  std::vector<Face> ordered;
  for(const auto patch:gpu.patches) {
    require((patch&63)==0 && (gpu.faces[patch>>6][3]>>20)==0,"cutout diagnostic must contain only unit patches");
    const auto found=remaining.find(gpu.faces[patch>>6]);
    require(found!=remaining.end(),"cutout draw order has a missing or duplicated CPU-oracle face");
    ordered.push_back(*found);remaining.erase(found);
  }
  require(remaining.empty(),"cutout draw order omitted an independent CPU-oracle face");
  const auto reordered=f.unit_mesh(ordered,c.min_y,c.height);
  const auto actual=f.render(gpu.gpu,camera,true,true),same_order=f.render(reordered.gpu,camera,true,true);
  require(actual.depth==same_order.depth,"identical cutout primitive order changed depth");
  for(unsigned target=0;target<4;++target)require(actual.mrt[target]==same_order.mrt[target],"identical cutout primitive order changed MRT");
  std::reverse(ordered.begin(),ordered.end());
  const auto reversed=f.unit_mesh(ordered,c.min_y,c.height);
  const auto other=f.render(reversed.gpu,camera,true,true);
  unsigned changed=0,equal_depth=0;
  for(std::size_t i=0;i<actual.depth.size();++i) {
    unsigned a=0,b=0;
    for(unsigned byte=0;byte<4;++byte) {a|=unsigned(actual.mrt[2][i*4+byte])<<(byte*8);b|=unsigned(other.mrt[2][i*4+byte])<<(byte*8);}
    if(a!=b) {
      ++changed;equal_depth+=actual.depth[i]==other.depth[i];
      require(actual.depth[i]==other.depth[i] && ((a>>3)&31)==((b>>3)&31),
          "reversing cutout order changed a non-tied surface or atlas layer");
    }
  }
  std::printf("world_mesh_cutout_order identical_order=byte_identical reversed_voxel_changes=%u exact_depth_ties=%u\n",changed,equal_depth);
  f.compare_raster("cutout_uv",gpu,reordered,camera,true,true);
}
void raster_cases(Fixture& f) {
  const auto grass=id(f,"grass"),stone=id(f,"stone");
  f.renderer.sources.clear();auto c=column(-2,3,-32,32);
  box(c,4,4,4,28,20,28,grass);
  auto merged=f.mesh(c);auto units=f.unit_mesh(f.verify("raster_grass_box",c,merged),c.min_y,c.height);
  constexpr float offsets[6][3]={{-46,5,7},{46,5,-7},{7,-46,5},{-7,46,5},{7,5,-46},{-7,5,46}};
  const float tx=float(c.x*32+16),ty=float(c.min_y+12),tz=float(c.z*32+16);
  for(unsigned direction=0;direction<6;++direction)for(unsigned mode=0;mode<3;++mode) {
    const auto& v=offsets[direction];const auto camera=look(tx+v[0],ty+v[1],tz+v[2],tx,ty,tz);
    const auto name=std::string("six_directions_")+std::to_string(direction);
    f.compare_raster(name.c_str(),merged,units,camera,mode!=0,mode==2);
  }
  f.compare_raster("distant_mip",merged,units,look(tx+47,ty+31,tz+130,tx,ty,tz),true,true);
  f.compare_raster("near_face_pom",merged,units,look(tx+8,ty+5,tz+27,tx,ty,tz),true,true);
  c=column();box(c,1,2,1,31,3,7,stone);box(c,1,2,7,7,3,31,stone);box(c,12,2,12,25,3,25,stone);
  merged=f.mesh(c);units=f.unit_mesh(f.verify("raster_concave",c,merged),c.min_y,c.height);
  f.compare_raster("concave_oblique",merged,units,look(47,32,43,15,2,15),true,true);
  cutout_case(f);
}
}
int main(int argc,char** argv) {
  std::setvbuf(stdout,nullptr,_IONBF,0);
  try {
    mesh_probe::atlas_mip_cases();
    if(argc==2 && std::string_view(argv[1])=="--mips-cpu")return 0;
    const bool batch_only=argc==2 && std::string_view(argv[1])=="--batch-only";
    mesh_probe::Fixture fixture(batch_only);
    if(argc==2 && std::string_view(argv[1])=="--frames-only") {mesh_probe::frames_cases(fixture);return 0;}
    if(argc==2 && std::string_view(argv[1])=="--greedy-output-timing") {
      mesh_probe::greedy_output_timing(fixture);
      mesh_probe::require(fixture.renderer.debug.errors.load()==0,"validation errors");return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--greedy-output-only") {
      mesh_probe::greedy_output_cases(fixture);
      mesh_probe::require(fixture.renderer.debug.errors.load()==0,"validation errors");return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--halo-only") {mesh_probe::halo_lifecycle_cases(fixture);return 0;}
    if(argc==2 && std::string_view(argv[1])=="--animation-only") {mesh_probe::atlas_animation_cases(fixture);return 0;}
    if(argc==2 && std::string_view(argv[1])=="--cutout-only") {mesh_probe::cutout_case(fixture);return 0;}
    if(batch_only) {mesh_probe::batch_cases(fixture);return 0;}
    if(argc==2 && std::string_view(argv[1])=="--atlas-only") {
      mesh_probe::atlas_filtering_cases(fixture);return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--seams-only") {
      mesh_probe::seam_cases(fixture);return 0;
    }
    mesh_probe::require(argc==1,"usage: octaryn_client_world_mesh_probe [--seams-only|--atlas-only|--mips-cpu|--batch-only|--frames-only|--cutout-only|--halo-only|--animation-only|--greedy-output-only|--greedy-output-timing]");
    mesh_probe::frames_cases(fixture);
    mesh_probe::atlas_filtering_cases(fixture);
    mesh_probe::atlas_animation_cases(fixture);
    mesh_probe::patch_coordinate_cases(fixture);
    mesh_probe::surface_cases(fixture);mesh_probe::greedy_output_cases(fixture);
    mesh_probe::relative_precision_cases(fixture);mesh_probe::seam_cases(fixture);
    mesh_probe::raster_cases(fixture);mesh_probe::binding_cases(fixture);mesh_probe::culling_cases(fixture);
    mesh_probe::halo_lifecycle_cases(fixture);
    mesh_probe::require(fixture.renderer.debug.errors.load()==0,"validation errors");
    std::puts("world_mesh_parity=passed production_mesher=1 production_raster=1 cpu_surface_oracle=1 windows=0");return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"world_mesh_parity=failed error=%s\n",error.what());return 1;
  }
}
