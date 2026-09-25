#pragma once
#include "BlockTransportPlantProbe.h"
#include "BlockTransportWork.h"
#include <algorithm>
namespace mesh_probe::leaf_probe {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using Color=std::array<double,3>;
constexpr unsigned Samples=4096,Geometry=211,Radiance=223,Frame=17;
constexpr int Anchor[3]={-48,-24,112};
struct Query {Words face,info;};
struct Result {BlockSurfaceKey key;Pixel reflectance,origin,direction,position;Words proof;};
static_assert(sizeof(Query)==32 && sizeof(Result)==96);
inline Pixel point(unsigned face,double u,double v) {
  const std::array<std::array<double,3>,6> local{{{0,1-v,u},{1,1-v,1-u},{u,0,v},
      {u,1,1-v},{1-u,1-v,0},{u,1-v,1}}};
  return {float(Anchor[0]+local[face][0]),float(Anchor[1]+local[face][1]),float(Anchor[2]+local[face][2]),0};
}
inline Pixel normal(unsigned face) {Pixel n{};n[face/2]=face%2?1.f:-1.f;return n;}
inline std::array<double,2> uv(const Pixel& position,unsigned face) {
  const double x=position[0]-Anchor[0],y=position[1]-Anchor[1],z=position[2]-Anchor[2];
  const std::array<std::array<double,2>,6> coordinates{{{z,1-y},{1-z,1-y},{x,z},{x,1-z},{1-x,1-y},{x,1-y}}};
  return coordinates[face];
}
inline double linear(unsigned value) {
  const double c=double(value)/255;return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);
}
struct Oracle {
  std::array<bool,1024> mask{};Color rho{};unsigned count{};
  Oracle(SDL_Surface* color,SDL_Surface* specular,unsigned layer) {
    require(color && specular && color->w==32*29 && color->h==32 && specular->w==color->w && specular->h==32,
        "BT leaf original PNG dimensions");
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
      const auto* p=static_cast<const Uint8*>(color->pixels)+y*unsigned(color->pitch)+(layer*32+x)*4;
      const auto* s=static_cast<const Uint8*>(specular->pixels)+y*unsigned(specular->pitch)+(layer*32+x)*4;
      // The catalog fixture is binary alpha: opaque RGB is unchanged by upload dilation.
      require(p[3]==0 || p[3]==255,"BT independent leaf PNG oracle requires binary catalog mask");
      if(p[3]==0)continue;
      mask[y*32+x]=true;++count;
      if(s[1]<230)for(unsigned c=0;c<3;++c)rho[c]+=.96*linear(p[c]);
    }
    require(count>0 && count<1024,"BT leaf fixture must contain solid texels and holes");
    for(auto& value:rho)value/=count;
  }
  bool owns(const Pixel& position,unsigned face)const {
    const auto t=uv(position,face);
    if(t[0]<0 || t[1]<0 || t[0]>=1 || t[1]>=1)return false;
    return mask[unsigned(t[1]*32)*32+unsigned(t[0]*32)];
  }
  static double source(double u,double v) {
    const double square=(u-.13)*(u-.13)+(v-.79)*(v-.79)+.64;
    const double fade=std::pow(1-square*square/std::pow(24.,4),2);
    return .8*fade/(3.14159265358979323846*square*std::sqrt(square));
  }
  double irradiance(unsigned subdivisions,bool conditional=true)const {
    double sum=0;unsigned texels=0;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
      if(conditional && !mask[y*32+x])continue;
      ++texels;
      for(unsigned sy=0;sy<subdivisions;++sy)for(unsigned sx=0;sx<subdivisions;++sx)
        sum+=source((x+(sx+.5)/subdivisions)/32,(y+(sy+.5)/subdivisions)/32);
    }
    return sum/(double(texels)*subdivisions*subdivisions);
  }
};
void energy(Fixture&,const Oracle&,const std::array<Pixel,6>&);
}
