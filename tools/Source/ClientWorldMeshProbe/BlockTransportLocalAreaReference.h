#pragma once
#include "BlockTransportPlantProbe.h"
#include "LocalLight.h"
#include <algorithm>

namespace mesh_probe::local_area {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
using Vector=std::array<double,3>;
constexpr unsigned Samples=16384,Geometry=307,Radiance=311,Frame=19;
constexpr double Pi=3.14159265358979323846;
constexpr Vector Anchor{-48,-24,112};
struct Cell {double x0,x1,y0,y1;};
struct Domain {
  BlockSurfaceKey key{-48,-24,112,3};float metadata=1;
  std::vector<Cell> cells;
  Vector normal{0,1,0};
  bool plant=false;unsigned plane=0;
  Domain(SDL_Surface* png,unsigned layer=0,bool mask=false,bool crossed=false,unsigned face=0):plant(crossed),plane(face/2) {
    if(plant) {
      key.direction=plant_probe::tag(layer,plane,face%2);metadata=0;
      const double sign=face%2?-1.:1.;normal={(plane?1:-1)*sign/std::sqrt(2.),0,sign/std::sqrt(2.)};
    } else if(mask)metadata=-float(layer+1);
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
      if(mask && !plant_probe::opaque(png,layer,x,y))continue;
      if(plant)cells.push_back({std::max(0.,(x-.5)/31),std::min(1.,(x+.5)/31),
          std::max(0.,(y-.5)/31),std::min(1.,(y+.5)/31)});
      else cells.push_back({x/32.,(x+1)/32.,y/32.,(y+1)/32.});
    }
    require(!cells.empty(),"BT local area independent PNG domain is empty");
  }
  Vector point(double u,double v)const {
    if(plant)return {Anchor[0]+1-u,Anchor[1]+1-v,Anchor[2]+(plane?u:1-u)};
    return {Anchor[0]+u,Anchor[1]+1,Anchor[2]+1-v};
  }
  WorldLocalLight light(double u,double v,double height,Pixel color)const {
    auto p=point(u,v);WorldLocalLight result;
    for(unsigned c=0;c<3;++c)result.position_range[c]=float(p[c]+normal[c]*height);
    result.position_range[3]=24;result.color_intensity=color;result.axis_v_type[3]=0;return result;
  }
};
// Independent exact slab intersection with the fixture's translated twelve-triangle box.
inline bool blocked(Vector origin,Vector direction,double maximum) {
  constexpr Vector low{-47.75,-22.75,112.25},high{-47.25,-22.25,112.75};
  double enter=0,leave=maximum;
  for(unsigned axis=0;axis<3;++axis) {
    if(std::abs(direction[axis])<1e-15) {
      if(origin[axis]<low[axis] || origin[axis]>high[axis])return false;
    } else {
      double a=(low[axis]-origin[axis])/direction[axis],b=(high[axis]-origin[axis])/direction[axis];
      if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);
    }
  }
  return leave>=enter;
}
inline Vector source(const Domain& domain,Vector point,const std::vector<WorldLocalLight>& lights,bool occluded) {
  Vector sum{};
  for(const auto& light:lights) {
    Vector delta{},direction{};double square=0,cosine=0;
    for(unsigned c=0;c<3;++c) {delta[c]=light.position_range[c]-point[c];square+=delta[c]*delta[c];}
    const double distance=std::sqrt(square);if(distance<.001 || distance>=light.position_range[3])continue;
    for(unsigned c=0;c<3;++c) {direction[c]=delta[c]/distance;cosine+=direction[c]*domain.normal[c];}
    if(cosine<=0 || (occluded && blocked(point,direction,distance)))continue;
    const double fade=std::pow(1-square*square/std::pow(double(light.position_range[3]),4),2);
    const double scale=cosine*fade/(Pi*std::max(square,.01));
    for(unsigned c=0;c<3;++c)sum[c]+=light.color_intensity[c]*light.color_intensity[3]*scale;
  }
  return sum;
}
struct Reference {Vector mean{};double red_variance=0;};
inline Reference integrate(const Domain& domain,const std::vector<WorldLocalLight>& lights,bool occluded,unsigned subdivisions) {
  Reference result;double area=0,second=0;
  for(const auto cell:domain.cells) {
    const double weight=(cell.x1-cell.x0)*(cell.y1-cell.y0)/(subdivisions*subdivisions);
    for(unsigned y=0;y<subdivisions;++y)for(unsigned x=0;x<subdivisions;++x) {
      const auto p=domain.point(cell.x0+(cell.x1-cell.x0)*(x+.5)/subdivisions,
          cell.y0+(cell.y1-cell.y0)*(y+.5)/subdivisions);
      const auto value=source(domain,p,lights,occluded);area+=weight;
      for(unsigned c=0;c<3;++c)result.mean[c]+=value[c]*weight;
      second+=value[0]*value[0]*weight;
    }
  }
  require(area>0,"BT local area oracle has no area");for(auto& c:result.mean)c/=area;
  result.red_variance=std::max(0.,second/area-result.mean[0]*result.mean[0]);return result;
}
inline double relative(Vector a,Vector b) {
  double result=0;for(unsigned c=0;c<3;++c)result=std::max(result,std::abs(a[c]-b[c])/std::max(std::abs(b[c]),1e-6));
  return result;
}
}
