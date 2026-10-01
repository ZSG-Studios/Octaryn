#pragma once
#include "SceneResidency.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::scene_geometry::bounds {
inline bool valid(const Bounds& value) {
  for(unsigned i=0;i<3;++i)
    if(!std::isfinite(value[i]) || !std::isfinite(value[i+3]) || value[i]>value[i+3])return false;
  return true;
}
inline Bounds empty() {
  const auto infinity=std::numeric_limits<float>::infinity();
  return {infinity,infinity,infinity,-infinity,-infinity,-infinity};
}
inline void include(Bounds& a,const Bounds& b) {
  for(unsigned i=0;i<3;++i){a[i]=std::min(a[i],b[i]);a[i+3]=std::max(a[i+3],b[i+3]);}
}
inline bool overlap(const Bounds& a,const Bounds& b) {
  for(unsigned i=0;i<3;++i)if(a[i]>b[i+3] || b[i]>a[i+3])return false;
  return true;
}
inline bool near(const Bounds& a,const std::array<float,3>& point,float radius,bool ignore_vertical=false) {
  double squared{};
  for(unsigned i=0;i<3;++i) {
    if(ignore_vertical && i==1)continue;
    const double delta=std::max({double(a[i])-point[i],double(point[i])-a[i+3],0.});
    squared+=delta*delta;
  }
  return squared<=double(radius)*radius;
}
inline Bounds around(const std::array<float,3>& point,float radius) {
  return {point[0]-radius,point[1]-radius,point[2]-radius,
      point[0]+radius,point[1]+radius,point[2]+radius};
}
inline Bounds transform(const Bounds& box,const std::array<float,16>& matrix) {
  Bounds result=empty();
  for(unsigned corner=0;corner<8;++corner)for(unsigned row=0;row<3;++row) {
    double value=matrix[12+row],magnitude=std::abs(value);
    for(unsigned axis=0;axis<3;++axis) {
      const double term=double(matrix[axis*4+row])*box[axis+((corner&(1u<<axis))?3:0)];
      value+=term;magnitude+=std::abs(term);
    }
    // Bound FP32 vertex arithmetic too, including cancellation under mirrored/sheared transforms.
    const double padding=magnitude*8*std::numeric_limits<float>::epsilon();
    result[row]=std::min(result[row],std::nextafter(float(value-padding),-std::numeric_limits<float>::infinity()));
    result[row+3]=std::max(result[row+3],std::nextafter(float(value+padding),std::numeric_limits<float>::infinity()));
  }
  return result;
}
inline bool inverse(const std::array<float,16>& m,std::array<float,16>& result) {
  for(const float value:m)if(!std::isfinite(value))return false;
  if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1)return false;
  const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
  const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
  if(!std::isfinite(determinant) || determinant==0)return false;
  result={float((e*i-f*h)/determinant),float((f*g-d*i)/determinant),float((d*h-e*g)/determinant),0,
      float((c*h-b*i)/determinant),float((a*i-c*g)/determinant),float((b*g-a*h)/determinant),0,
      float((b*f-c*e)/determinant),float((c*d-a*f)/determinant),float((a*e-b*d)/determinant),0,0,0,0,1};
  for(unsigned row=0;row<3;++row) {
    double value{};for(unsigned axis=0;axis<3;++axis)value-=double(result[axis*4+row])*m[12+axis];
    result[12+row]=float(value);
  }
  for(const float value:result)if(!std::isfinite(value))return false;
  return true;
}
}
