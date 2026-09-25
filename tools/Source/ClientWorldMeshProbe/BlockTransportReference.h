#pragma once
#include "BlockTransportSetup.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportTypes.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace mesh_probe::transport_reference {
constexpr unsigned Faces=96,Side=4;
constexpr double Pi=3.14159265358979323846;
using Vector=std::array<double,3>;
using Matrix=std::array<std::array<double,Faces>,Faces>;
using Field=std::array<Vector,Faces>;
using Keys=std::array<BlockSurfaceKey,Faces>;
inline Keys keys() {
  Keys result{};const int low[3]={9,1,9},high[3]={13,5,13};unsigned index=0;
  for(unsigned face=0;face<6;++face)for(unsigned y=0;y<Side;++y)for(unsigned x=0;x<Side;++x) {
    const unsigned axis=face/2,u=axis==0?2:0,v=axis==1?2:1;int point[3]={9,1,9};
    point[axis]=face%2?low[axis]-1:high[axis];point[u]+=int(x);point[v]+=int(y);
    result[index++]={point[0],point[1],point[2],face};
  }
  return result;
}
inline Vector point(BlockSurfaceKey key,double x,double y) {
  Vector result{key.x+.5,key.y+.5,key.z+.5};const unsigned axis=key.direction/2;
  result[axis]+=key.direction%2?.5:-.5;
  result[axis==0?2:0]+=x-.5;result[axis==1?2:1]+=y-.5;return result;
}
inline double radical(unsigned index,unsigned base) {
  double value=0,scale=1;
  while(index) {scale/=base;value+=(index%base)*scale;index/=base;}return value;
}
inline double direct(BlockSurfaceKey key,Vector p) {
  const Vector source{10.3,3.5,10.7};Vector delta{};double squared=0;
  for(unsigned axis=0;axis<3;++axis){delta[axis]=source[axis]-p[axis];squared+=delta[axis]*delta[axis];}
  const double cosine=delta[key.direction/2]*(key.direction%2?1:-1)/std::sqrt(squared);
  const double fade=std::pow(1-squared*squared/std::pow(24.,4),2);
  return 20*std::max(cosine,0.)*fade/(Pi*squared);
}
inline unsigned hit(const Keys& surfaces,BlockSurfaceKey receiver,Vector p,double radial,double angular) {
  const unsigned axis=receiver.direction/2,u=axis==0?2:0,v=axis==1?2:1;
  Vector direction{};direction[axis]=(receiver.direction%2?1:-1)*std::sqrt(1-radial);
  direction[u]=std::sqrt(radial)*std::cos(2*Pi*angular);
  direction[v]=std::sqrt(radial)*std::sin(2*Pi*angular);
  const double low[3]={9,1,9},high[3]={13,5,13};double nearest=1e30;unsigned face=6;
  for(unsigned a=0;a<3;++a) {
    if(std::abs(direction[a])<1e-15)continue;
    const double t=((direction[a]>0?high[a]:low[a])-p[a])/direction[a];
    if(t>1e-10 && t<nearest){nearest=t;face=a*2+(direction[a]<0?1:0);}
  }
  require(face<6,"BT independent reference ray escaped sealed cube");
  const unsigned target_axis=face/2,tu=target_axis==0?2:0,tv=target_axis==1?2:1;
  const unsigned x=std::min(Side-1,unsigned(std::max(0.,std::floor(p[tu]+direction[tu]*nearest-low[tu]))));
  const unsigned y=std::min(Side-1,unsigned(std::max(0.,std::floor(p[tv]+direction[tv]*nearest-low[tv]))));
  const unsigned index=face*Side*Side+y*Side+x;
  require(surfaces[index].direction==face,"BT reference target orientation");return index;
}
struct Reference {Keys surfaces=keys();Matrix transport{};Field sources{},indirect{};};
inline Field solve(const Matrix& t,const Field& sources) {
  Field result{},outgoing=sources;
  for(auto& color:outgoing)for(auto& value:color)value*=.5;
  for(unsigned bounce=0;bounce<3;++bounce) {
    Field gathered{};
    for(unsigned row=0;row<Faces;++row)for(unsigned target=0;target<Faces;++target)
      for(unsigned c=0;c<3;++c)gathered[row][c]+=t[row][target]*outgoing[target][c];
    for(unsigned row=0;row<Faces;++row)for(unsigned c=0;c<3;++c) {
      result[row][c]+=gathered[row][c];outgoing[row][c]=.5*gathered[row][c];
    }
  }
  return result;
}
inline Reference integrate(WorldRenderer& renderer) {
  Reference result;Matrix coarse{};constexpr unsigned Rays=16384,Grid=128;
  const Vector color{.9,.4,.15};
  for(unsigned row=0;row<Faces;++row) {
    const auto start=std::chrono::steady_clock::now();const auto key=result.surfaces[row];
    double irradiance=0;
    for(unsigned y=0;y<Grid;++y)for(unsigned x=0;x<Grid;++x)
      irradiance+=direct(key,point(key,(x+.5)/Grid,(y+.5)/Grid));
    for(unsigned c=0;c<3;++c)result.sources[row][c]=color[c]*irradiance/(Grid*Grid);
    for(unsigned sample=1;sample<=Rays;++sample) {
      const auto p=point(key,radical(sample,2),radical(sample,3));
      const unsigned target=hit(result.surfaces,key,p,radical(sample,5),radical(sample,7));
      result.transport[row][target]+=1./Rays;
      if(sample<=Rays/2)coarse[row][target]+=2./Rays;
    }
    block_transport_complete(renderer,start);
  }
  result.indirect=solve(result.transport,result.sources);const auto half=solve(coarse,result.sources);
  double sum=0,error=0;
  for(unsigned row=0;row<Faces;++row)for(unsigned c=0;c<3;++c) {
    sum+=result.indirect[row][c]*result.indirect[row][c];
    error+=std::pow(result.indirect[row][c]-half[row][c],2);
  }
  const double difference=std::sqrt(error/sum);
  require(difference<.01,"BT independent geometric reference did not converge");
  std::printf("block_transport_reference=passed independent_halton=1 double_precision=1 complete_room=1 rows=%u rays_per_row=%u direct_grid=%u reference_refinement_rms=%.9g\n",
      Faces,Rays,Grid,difference);return result;
}
}
