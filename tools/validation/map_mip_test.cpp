#include "MapMipmaps.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void check(bool value,const char* label) {
  ++checks;if(!value) {std::fprintf(stderr,"map_mips failed: %s\n",label);std::exit(1);}
}
void near(int value,int target,int tolerance,const char* label) {
  if(std::abs(value-target)>tolerance)std::fprintf(stderr,"%s expected=%d got=%d\n",label,target,value);
  check(std::abs(value-target)<=tolerance,label);
}
}
int main() {
  MapMipOptions base;MapDecodedImage image{2,1,{255,255,255,255,0,0,0,255}};
  auto levels=build_map_mips(image,base);
  check(levels.size()==2,"two levels");near(levels.back().rgba[0],188,1,"linear-light midpoint");
  image.rgba={255,0,0,255,0,0,255,0};base.alpha_weighted=true;
  levels=build_map_mips(image,base);
  check(levels[0].rgba[4]==255 && levels[0].rgba[6]==0 && levels[0].rgba[7]==0,"dilate only transparent RGB");
  near(levels[1].rgba[0],255,0,"alpha-weighted red");near(levels[1].rgba[2],0,0,"no blue transparent fringe");
  near(levels[1].rgba[3],128,0,"straight alpha mean");
  base.alpha_weighted=false;levels=build_map_mips(image,base);
  near(levels[1].rgba[0],188,1,"opaque ignores texture alpha");
  near(levels[1].rgba[2],188,1,"opaque retains RGB of zero-alpha texel");
  MapMipOptions normal;normal.role=MapMipRole::Normal;
  image.rgba={255,128,128,255,128,128,255,255};levels=build_map_mips(image,normal);
  near(levels[1].rgba[0],218,1,"normalized mean normal x");near(levels[1].rgba[2],218,1,"normalized mean normal z");
  MapMipOptions mr;mr.role=MapMipRole::MetalRough;
  image.rgba={0,0,0,255,255,255,255,255};levels=build_map_mips(image,mr);
  near(levels[1].rgba[0],128,0,"MR red linear");near(levels[1].rgba[1],180,1,"roughness RMS");
  near(levels[1].rgba[2],128,0,"metallic linear");
  MapMipOptions ao;ao.role=MapMipRole::Occlusion;levels=build_map_mips(image,ao);
  near(levels[1].rgba[1],128,0,"data role not roughness filtered");
  MapMipOptions emission;emission.role=MapMipRole::Emissive;levels=build_map_mips(image,emission);
  near(levels[1].rgba[0],188,1,"emissive sRGB linear-light");
  image={3,1,{255,255,255,255,0,0,0,255,0,0,0,255}};
  levels=build_map_mips(image,ao);near(levels[1].rgba[0],85,0,"odd width includes every source texel");
  image={1,3,{255,255,255,255,0,0,0,255,0,0,0,255}};
  levels=build_map_mips(image,ao);near(levels[1].rgba[0],85,0,"odd height includes every source texel");
  MapMaterial material;material.alpha_mode=MapAlphaMode::Mask;material.alpha_cutoff=.5f;
  auto mask=map_mip_options(material,0);
  image={4,1,{255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0}};
  levels=build_map_mips(image,mask);
  check(levels.back().rgba[3]>=128,"coverage smallest mip retains possible silhouette");
  material.base_color[3]=.5f;auto factor_mask=map_mip_options(material,0);
  levels=build_map_mips(image,factor_mask);
  check(levels.back().rgba[3]==255,"coverage respects base alpha factor");
  std::map<MapMipOptions,int> keys;keys[mask]=1;keys[factor_mask]=2;keys[mr]=3;keys[normal]=4;keys[ao]=5;
  material.alpha_cutoff=.25f;keys[map_mip_options(material,0)]=6;
  check(keys.size()==6,"variant roles factors cutoffs remain distinct");
  material.alpha_mode=MapAlphaMode::Opaque;
  check(!map_mip_options(material,0).alpha_weighted,"opaque options");
  material.alpha_mode=MapAlphaMode::Blend;
  check(map_mip_options(material,0).alpha_weighted && !map_mip_options(material,0).preserve_coverage,"blend options");
  check(!map_mip_options(material,4).alpha_weighted,"emission ignores alpha");
  check(build_map_mips(MapDecodedImage{},base).empty(),"empty input rejected");
  image={2,2,{1,2,3,4}};check(build_map_mips(image,base).empty(),"bad byte count rejected");
  std::printf("map_mip_checks=%u passed\n",checks);
}
