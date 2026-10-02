#include "SceneHullDecomposition.h"
#include <box3d/collision.h>
#include <algorithm>
#include <cmath>

namespace octaryn::character_motion {
namespace {
struct Face {b3Vec3 normal{};float offset{};};
struct Point {b3Vec3 position{};float x{},y{};};
float turn(const Point& a,const Point& b,const Point& c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
std::vector<Point> polygon(const Face& face,std::span<const b3Vec3> points,float tolerance) {
  const auto axis=std::abs(face.normal.x)<.8f?b3Vec3{1,0,0}:b3Vec3{0,1,0};
  const auto u=b3Normalize(b3Cross(face.normal,axis));const auto v=b3Cross(face.normal,u);
  std::vector<Point> projected;
  for(const auto point:points)if(std::abs(b3Dot(face.normal,point)-face.offset)<=tolerance)
    projected.push_back({point,b3Dot(u,point),b3Dot(v,point)});
  if(projected.size()<3)return {};
  std::sort(projected.begin(),projected.end(),[](const auto& a,const auto& b){return a.x==b.x?a.y<b.y:a.x<b.x;});
  std::vector<Point> hull;
  for(const auto& point:projected) {
    while(hull.size()>1 && turn(hull[hull.size()-2],hull.back(),point)<=0)hull.pop_back();
    hull.push_back(point);
  }
  const auto lower=hull.size();
  for(auto it=projected.rbegin()+1;it!=projected.rend();++it) {
    while(hull.size()>lower && turn(hull[hull.size()-2],hull.back(),*it)<=0)hull.pop_back();
    hull.push_back(*it);
  }
  if(!hull.empty())hull.pop_back();return hull;
}
}
bool attach_decomposed_scene_hull(b3BodyId body,std::vector<b3HullData*>& retained,std::span<const b3Vec3> points,const b3ShapeDef& def) {
  if(points.size()<4 || points.size()>128)return false;
  b3Vec3 center{};float diameter{};
  for(const auto p:points) {center=b3Add(center,p);for(const auto q:points)diameter=std::max(diameter,b3Distance(p,q));}
  center=b3MulSV(1.f/float(points.size()),center);const float epsilon=std::max(1e-7f,diameter*1e-6f);
  std::vector<Face> faces;
  for(std::size_t i=0;i<points.size();++i)for(std::size_t j=i+1;j<points.size();++j)for(std::size_t k=j+1;k<points.size();++k) {
    auto n=b3Cross(b3Sub(points[j],points[i]),b3Sub(points[k],points[i]));if(b3Length(n)<epsilon*epsilon)continue;
    n=b3Normalize(n);float d=b3Dot(n,points[i]);bool positive{},negative{};
    for(const auto p:points) {const float distance=b3Dot(n,p)-d;positive=positive || distance>epsilon;negative=negative || distance<-epsilon;if(positive && negative)break;}
    if(positive==negative)continue;
    if(positive) {n=b3Neg(n);d=-d;}
    if(std::any_of(faces.begin(),faces.end(),[&](const auto& f){return b3Dot(f.normal,n)>1-1e-6f && std::abs(f.offset-d)<=epsilon;}))continue;
    faces.push_back({n,d});
  }
  if(faces.size()<4 || faces.size()>256)return false;
  std::vector<b3HullData*> hulls;
  const auto reject=[&] {for(auto* hull:hulls)b3DestroyHull(hull);return false;};
  for(const auto& face:faces) {
    const auto boundary=polygon(face,points,epsilon);if(boundary.size()<3)return reject();
    for(std::size_t i=1;i+1<boundary.size();++i) {
      const b3Vec3 tetra[]={center,boundary[0].position,boundary[i].position,boundary[i+1].position};
      auto* hull=b3CreateHull(tetra,4,4);if(!hull || hulls.size()>=1024) {if(hull)b3DestroyHull(hull);return reject();}
      hulls.push_back(hull);
    }
  }
  if(hulls.empty())return false;
  retained.insert(retained.end(),hulls.begin(),hulls.end());
  for(auto* hull:hulls)if(!b3Shape_IsValid(b3CreateHullShape(body,&def,hull)))return false;
  return true;
}
}
