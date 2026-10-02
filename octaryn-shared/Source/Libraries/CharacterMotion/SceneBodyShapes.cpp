#include "SceneBodyShapes.h"
#include "SceneHullDecomposition.h"
#include <box3d/collision.h>
#include <algorithm>
#include <cmath>
namespace octaryn::character_motion {
namespace {
b3Vec3 vector(const float* p) {return {p[0],p[1],p[2]};}
bool finite(const float* p,unsigned n) {for(unsigned i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
b3Quat rotation(const float* q) {return {{q[0],q[1],q[2]},q[3]};}
bool valid_rotation(const float* q) {return finite(q,4) && std::abs(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]-1)<0.01f;}
}
bool attach_scene_body_shape(b3BodyId body, std::vector<b3HullData*>& hulls,const octaryn_scene_body_shape& s,const b3ShapeDef& def) {
  if(!finite(s.local_position,3) || !valid_rotation(s.local_rotation))return false;
  const b3Transform transform{vector(s.local_position),rotation(s.local_rotation)};
  b3ShapeId id{};
  if(s.kind==1) {
    if(!finite(s.half_extents,3) || std::min({s.half_extents[0],s.half_extents[1],s.half_extents[2]})<=0)return false;
    const auto box=b3MakeBoxHull(s.half_extents[0],s.half_extents[1],s.half_extents[2]);
    auto* hull=b3CloneAndTransformHull(&box.base,transform,b3Vec3_one);
    if(!hull)return false;hulls.push_back(hull);id=b3CreateHullShape(body,&def,hull);
  }else if(s.kind==2) {
    if(!std::isfinite(s.radius) || s.radius<=0)return false;
    const b3Sphere sphere{vector(s.local_position),s.radius};id=b3CreateSphereShape(body,&def,&sphere);
  }else if(s.kind==3) {
    if(!finite(s.capsule_a,3) || !finite(s.capsule_b,3) || !std::isfinite(s.radius) || s.radius<=0)return false;
    const b3Capsule capsule{b3TransformPoint(transform,vector(s.capsule_a)),b3TransformPoint(transform,vector(s.capsule_b)),s.radius};
    id=b3CreateCapsuleShape(body,&def,&capsule);
  }else if(s.kind==4) {
    if(!s.points || s.point_count<4 || s.point_count>1024 || !finite(s.points,s.point_count*3))return false;
    std::vector<b3Vec3> points;points.reserve(s.point_count);
    for(unsigned i=0;i<s.point_count;++i)points.push_back(b3TransformPoint(transform,vector(s.points+i*3)));
    auto* hull=b3CreateHull(points.data(),int(points.size()),int(points.size()));
    if(!hull)return attach_decomposed_scene_hull(body,hulls,points,def);
    hulls.push_back(hull);id=b3CreateHullShape(body,&def,hull);
  }else return false;
  return b3Shape_IsValid(id);
}
}
