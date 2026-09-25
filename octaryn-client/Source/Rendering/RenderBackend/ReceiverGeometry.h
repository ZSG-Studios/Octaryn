#pragma once
#include <array>
#include <cmath>

namespace octaryn::client::rendering {
using ReceiverVector=std::array<float,3>;
struct ReceiverGeometry {ReceiverVector position{},normal{};};
inline ReceiverVector receiver_cross(ReceiverVector a,ReceiverVector b) {
  return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline float receiver_dot(ReceiverVector a,ReceiverVector b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline ReceiverVector receiver_rotate(ReceiverVector p,float cosine,float sine) {
  return {cosine*p[0]-sine*p[2],p[1],sine*p[0]+cosine*p[2]};
}
inline bool player_receiver_geometry(const std::array<ReceiverVector,4>& columns,ReceiverVector position,
    ReceiverVector normal,ReceiverVector feet,float cosine,float sine,ReceiverGeometry& result) {
  ReceiverVector point=columns[3],direction{};
  for(unsigned axis=0;axis<3;++axis)for(unsigned column=0;column<3;++column)
    point[axis]+=columns[column][axis]*position[column];
  const std::array<ReceiverVector,3> cofactors{receiver_cross(columns[1],columns[2]),
      receiver_cross(columns[2],columns[0]),receiver_cross(columns[0],columns[1])};
  const float sign=receiver_dot(columns[0],cofactors[0])<0?-1.f:1.f;
  for(unsigned axis=0;axis<3;++axis)for(unsigned column=0;column<3;++column)
    direction[axis]+=cofactors[column][axis]*normal[column]*sign;
  point=receiver_rotate(point,cosine,sine);direction=receiver_rotate(direction,cosine,sine);
  const float length=std::sqrt(receiver_dot(direction,direction));
  if(!std::isfinite(length) || length<1e-8f)return false;
  for(unsigned axis=0;axis<3;++axis) {
    result.position[axis]=feet[axis]+point[axis];result.normal[axis]=direction[axis]/length;
    if(!std::isfinite(result.position[axis]))return false;
  }
  return true;
}
inline ReceiverGeometry item_receiver_geometry(ReceiverVector position,float phase,float time,unsigned face,bool sprite) {
  constexpr ReceiverVector normals[6]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
  const float angle=time*1.6f+phase;
  ReceiverGeometry result;result.normal=receiver_rotate(normals[sprite?5:face],std::cos(angle),std::sin(angle));
  const float offset=sprite?0:.125f;
  for(unsigned axis=0;axis<3;++axis)result.position[axis]=position[axis]+result.normal[axis]*offset;
  result.position[1]+=.1f+.025f*std::sin(time*2.5f+phase);
  return result;
}
}
