#include "GeometryTransform.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
float scale_bound(const std::array<float,12>& matrix) {
  double rows{},columns{};
  for(unsigned a=0;a<3;++a) {
    double row{},column{};
    for(unsigned b=0;b<3;++b) {row+=std::abs(double(matrix[a*4+b]));column+=std::abs(double(matrix[b*4+a]));}
    rows=std::max(rows,row);columns=std::max(columns,column);
  }
  return float(std::sqrt(rows*columns));
}
}
bool geometry_transform(const std::array<float,16>& m,GeometryTransform& output,std::string& error) {
  const auto fail=[&](const char* reason){error=reason;return false;};
  for(float value:m)if(!std::isfinite(value))return fail("geometry transform is nonfinite");
  if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1)return fail("geometry transform must be affine");
  const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
  const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
  if(!std::isfinite(determinant) || std::abs(determinant)<=1e-20)return fail("geometry transform is singular");
  GeometryTransform next;
  const double inverse[9]={(e*i-f*h)/determinant,(c*h-b*i)/determinant,(b*f-c*e)/determinant,
      (f*g-d*i)/determinant,(a*i-c*g)/determinant,(c*d-a*f)/determinant,
      (d*h-e*g)/determinant,(b*g-a*h)/determinant,(a*e-b*d)/determinant};
  for(unsigned row=0;row<3;++row) {
    double translation{};
    for(unsigned column=0;column<3;++column) {
      next.world[row*4+column]=m[column*4+row];next.inverse[row*4+column]=float(inverse[row*3+column]);
      next.normal[row*4+column]=float(inverse[column*3+row]);translation-=inverse[row*3+column]*m[12+column];
    }
    next.world[row*4+3]=m[12+row];next.inverse[row*4+3]=float(translation);
  }
  next.scale=scale_bound(next.world);next.inverse_scale=scale_bound(next.inverse);next.orientation=determinant<0?-1.f:1.f;
  for(const auto* matrix:{&next.world,&next.inverse,&next.normal})for(float value:*matrix)
    if(!std::isfinite(value))return fail("geometry transform inverse exceeds floating point range");
  if(!std::isfinite(next.scale*next.inverse_scale))return fail("geometry transform scale exceeds floating point range");
  output=next;error.clear();return true;
}
std::array<float,3> geometry_transform_point(const std::array<float,12>& matrix,const std::array<float,3>& point) {
  std::array<float,3> result{};
  for(unsigned row=0;row<3;++row)result[row]=matrix[row*4]*point[0]+matrix[row*4+1]*point[1]+matrix[row*4+2]*point[2]+matrix[row*4+3];
  return result;
}
std::array<float,6> geometry_transform_bounds(const GeometryTransform& transform,const std::array<float,6>& bounds) {
  const auto far=std::numeric_limits<float>::max();std::array<float,6> result{far,far,far,-far,-far,-far};
  for(unsigned corner=0;corner<8;++corner) {
    const auto point=geometry_transform_point(transform.world,{bounds[(corner&1)?3:0],bounds[(corner&2)?4:1],bounds[(corner&4)?5:2]});
    for(unsigned axis=0;axis<3;++axis) {result[axis]=std::min(result[axis],point[axis]);result[axis+3]=std::max(result[axis+3],point[axis]);}
  }
  return result;
}
SelectionView geometry_local_view(const GeometryTransform& transform,const SelectionView& world) {
  auto local=world;const auto eye=geometry_transform_point(transform.inverse,{world.eye[0],world.eye[1],world.eye[2]});
  std::copy(eye.begin(),eye.end(),local.eye);
  local.focal_pixels*=transform.scale*transform.inverse_scale;
  // Affine error projection needs transformed group bounds; retain full detail meanwhile.
  local.error_pixels=0;
  for(unsigned plane=0;plane<6;++plane)for(unsigned column=0;column<4;++column) {
    float value=column==3?world.planes[plane][3]:0;
    for(unsigned row=0;row<3;++row)value+=world.planes[plane][row]*transform.world[row*4+column];
    local.planes[plane][column]=value;
  }
  return local;
}
}
