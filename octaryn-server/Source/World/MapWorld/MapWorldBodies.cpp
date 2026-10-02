#include "MapWorldBodies.h"
#include "MapWorldSession.h"
#include "MeshCollisionWorld.h"
#include "SceneBodyShapes.h"
#include <box3d/box3d.h>
#include <box3d/collision.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace octaryn::server::map_world {
namespace {
b3Vec3 vector(const float* p) {return {p[0],p[1],p[2]};}
bool finite(const float* p,unsigned n) {for(unsigned i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
b3Quat rotation(const float* q) {return {{q[0],q[1],q[2]},q[3]};}
bool valid_rotation(const float* q) {return finite(q,4) && std::abs(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]-1)<0.01f;}
void copy(float* p,b3Vec3 v) {p[0]=v.x;p[1]=v.y;p[2]=v.z;}
struct Body {
  uint64_t source{};
  b3BodyId id{};
  std::vector<b3HullData*> hulls;
  b3Vec3 frozen_velocity{},frozen_angular{};
  bool frozen{},frozen_awake{};
  ~Body() {if(b3Body_IsValid(id))b3DestroyBody(id);for(auto* h:hulls)b3DestroyHull(h);}
};

}
struct SceneBodies::State {
  b3WorldId world{};
  b3BodyId anchor{};
  b3JointId joint{};
  uint64_t grabbed{},next{1};
  float grab_force{};
  double accumulator{};
  std::unordered_map<uint64_t,std::unique_ptr<Body>> bodies;
  explicit State(b3WorldId w):world(w) {auto def=b3DefaultBodyDef();anchor=b3CreateBody(world,&def);}
  ~State() {if(b3Joint_IsValid(joint))b3DestroyJoint(joint,true);bodies.clear();if(b3Body_IsValid(anchor))b3DestroyBody(anchor);}
};
SceneBodies::SceneBodies(void* collision):state_(std::make_unique<State>(static_cast<character_motion::MeshCollisionWorld*>(collision)->world)) {}
SceneBodies::~SceneBodies()=default;
int SceneBodies::create(const octaryn_scene_body_desc& d,uint64_t& result) {
  result=0;
  if(!d.source_id || !d.shapes || !d.shape_count || d.shape_count>256 || !finite(d.position,3) || !valid_rotation(d.rotation) ||
      !std::isfinite(d.mass) || d.mass<=0 || !finite(&d.mass,5) || d.linear_damping<0 || d.angular_damping<0 || d.friction<0 || d.restitution<0 || d.restitution>1)return -1;
  for(const auto& [handle,body]:state_->bodies)if(body->source==d.source_id)return -1;
  auto body=std::make_unique<Body>();body->source=d.source_id;
  auto def=b3DefaultBodyDef();def.type=b3_dynamicBody;def.position=vector(d.position);def.rotation=rotation(d.rotation);
  def.linearDamping=d.linear_damping;def.angularDamping=d.angular_damping;
  body->id=b3CreateBody(state_->world,&def);if(!b3Body_IsValid(body->id))return -1;
  auto sd=b3DefaultShapeDef();sd.baseMaterial.friction=d.friction;sd.baseMaterial.restitution=d.restitution;
  for(unsigned i=0;i<d.shape_count;++i)if(!character_motion::attach_scene_body_shape(body->id,body->hulls,d.shapes[i],sd))return -1;
  auto mass=b3Body_GetMassData(body->id);
  if(!finite(d.center,3) || !finite(d.inertia,9))return -1;
  const bool authored=d.inertia[0]>0 && d.inertia[4]>0 && d.inertia[8]>0;
  if(authored)mass.inertia={{d.inertia[0],d.inertia[3],d.inertia[6]},{d.inertia[1],d.inertia[4],d.inertia[7]},{d.inertia[2],d.inertia[5],d.inertia[8]}};
  else {
    if(!(mass.mass>0))return -1;
    const float scale=d.mass/mass.mass;mass.inertia.cx=b3MulSV(scale,mass.inertia.cx);mass.inertia.cy=b3MulSV(scale,mass.inertia.cy);mass.inertia.cz=b3MulSV(scale,mass.inertia.cz);
  }
  mass.mass=d.mass;mass.center=vector(d.center);b3Body_SetMassData(body->id,mass);
  result=state_->next++;state_->bodies.emplace(result,std::move(body));return 0;
}
int SceneBodies::remove(uint64_t handle) {if(state_->grabbed==handle)release();return state_->bodies.erase(handle)?0:1;}
int SceneBodies::pose(uint64_t handle,octaryn_scene_body_pose& p) const {
  const auto it=state_->bodies.find(handle);if(it==state_->bodies.end())return 1;
  const auto& b=*it->second;const auto t=b3Body_GetTransform(b.id);p={};p.handle=handle;p.source_id=b.source;
  copy(p.position,t.p);copy(p.rotation,t.q.v);p.rotation[3]=t.q.s;copy(p.velocity,b.frozen?b.frozen_velocity:b3Body_GetLinearVelocity(b.id));copy(p.angular_velocity,b.frozen?b.frozen_angular:b3Body_GetAngularVelocity(b.id));
  p.flags=(!(b.frozen?b.frozen_awake:b3Body_IsAwake(b.id))?1u:0u)|(state_->grabbed==handle?2u:0u)|(b.frozen?8u:0u);return 0;
}
int SceneBodies::ray(const float* origin,const float* direction,float distance,octaryn_scene_body_hit& out) const {
  if(!origin || !direction || !finite(origin,3) || !finite(direction,3) || !std::isfinite(distance) || distance<=0)return -1;
  const auto dir=vector(direction);const float length=b3Length(dir);if(length<0.00001f)return -1;
  const auto hit=b3World_CastRayClosest(state_->world,vector(origin),b3MulSV(distance/length,dir),b3DefaultQueryFilter());
  if(!hit.hit)return 1;const auto body=b3Shape_GetBody(hit.shapeId);
  for(const auto& [handle,b]:state_->bodies)if(B3_ID_EQUALS(b->id,body)) {
    out={};out.handle=handle;out.source_id=b->source;copy(out.point,hit.point);copy(out.normal,hit.normal);out.distance=hit.fraction*distance;return 0;
  }
  return 1;
}
int SceneBodies::grab(uint64_t handle,const float* point,const float* target,float force) {
  if(!point || !target || !finite(point,3) || !finite(target,3) || !std::isfinite(force) || force<=0)return -1;
  const auto it=state_->bodies.find(handle);if(it==state_->bodies.end())return 1;release();
  auto def=b3DefaultMotorJointDef();def.base.bodyIdA=state_->anchor;def.base.bodyIdB=it->second->id;
  def.base.localFrameA.p=vector(target);def.base.localFrameB.p=b3Body_GetLocalPoint(it->second->id,vector(point));
  def.linearHertz=8;def.linearDampingRatio=1;def.maxSpringForce=force;def.angularHertz=0;def.maxSpringTorque=0;
  state_->joint=b3CreateMotorJoint(state_->world,&def);if(!b3Joint_IsValid(state_->joint))return -1;
  state_->grabbed=handle;state_->grab_force=force;b3Joint_WakeBodies(state_->joint);return 0;
}
int SceneBodies::move(const float* target) {
  if(!target || !finite(target,3))return -1;if(!b3Joint_IsValid(state_->joint))return 1;
  auto frame=b3Joint_GetLocalFrameA(state_->joint);frame.p=vector(target);b3Joint_SetLocalFrameA(state_->joint,frame);b3Joint_WakeBodies(state_->joint);return 0;
}
void SceneBodies::release() {if(b3Joint_IsValid(state_->joint))b3DestroyJoint(state_->joint,true);state_->joint={};state_->grabbed=0;state_->grab_force=0;}
void SceneBodies::suspend(uint64_t handle,bool suspended) {
  auto& b=*state_->bodies.at(handle);if(b.frozen==suspended)return;
  if(suspended) {b.frozen_velocity=b3Body_GetLinearVelocity(b.id);b.frozen_angular=b3Body_GetAngularVelocity(b.id);b.frozen_awake=b3Body_IsAwake(b.id);b3Body_Disable(b.id);}
  else {b3Body_Enable(b.id);b3Body_SetLinearVelocity(b.id,b.frozen_velocity);b3Body_SetAngularVelocity(b.id,b.frozen_angular);b3Body_SetAwake(b.id,b.frozen_awake);}
  b.frozen=suspended;
}
std::vector<std::pair<uint64_t,std::array<float,6>>> SceneBodies::bounds(double dt) const {
  std::vector<std::pair<uint64_t,std::array<float,6>>> out;out.reserve(state_->bodies.size());
  for(const auto& [handle,body]:state_->bodies) {
    const auto aabb=b3Body_ComputeAABB(body->id);const auto velocity=body->frozen?body->frozen_velocity:b3Body_GetLinearVelocity(body->id);
    const float force=state_->grabbed==handle?state_->grab_force/b3Body_GetMass(body->id):0;
    const float margin=1+float(b3Length(velocity)*dt+(10+force)*dt*dt);
    out.push_back({handle,{aabb.lowerBound.x-margin,aabb.lowerBound.y-margin,aabb.lowerBound.z-margin,
        aabb.upperBound.x+margin,aabb.upperBound.y+margin,aabb.upperBound.z+margin}});
  }
  return out;
}
int SceneBodies::step(double dt) {
  if(!std::isfinite(dt) || dt<=0 || dt>0.25)return -1;
  state_->accumulator+=dt;constexpr double fixed=1.0/120;
  while(state_->accumulator>=fixed) {b3World_Step(state_->world,float(fixed),4);state_->accumulator-=fixed;}return 0;
}
}
