#include "MapWorldBodies.h"
#include "MeshCollisionWorld.h"
#include "MeshCharacterStep.h"
#include "SceneHullDecomposition.h"
#include <box3d/box3d.h>
#include <box3d/collision.h>
#include <cstdio>
#include <cmath>
#include <stdexcept>

using octaryn::server::map_world::SceneBodies;
namespace {
void require(bool condition,const char* reason) {if(!condition)throw std::runtime_error(reason);}
octaryn_scene_body_pose pose(SceneBodies& bodies,uint64_t handle) {octaryn_scene_body_pose p{};require(bodies.pose(handle,p)==0,"pose missing");return p;}
void step(SceneBodies& bodies,unsigned frames) {for(unsigned i=0;i<frames;++i)require(bodies.step(1.0/60)==0,"step failed");}
octaryn_scene_body_desc desc(uint64_t source,const octaryn_scene_body_shape* shapes,unsigned count) {
  octaryn_scene_body_desc d{};d.source_id=source;d.shapes=shapes;d.shape_count=count;d.position[1]=3;d.rotation[3]=1;
  d.mass=1;d.friction=.6f;d.restitution=.1f;d.linear_damping=.05f;d.angular_damping=.1f;return d;
}
}
int main() {
  try {
    octaryn::character_motion::MeshCollisionWorld collision;
    auto wd=b3DefaultWorldDef();wd.gravity={0,-9.81f,0};collision.world=b3CreateWorld(&wd);
    auto bd=b3DefaultBodyDef();collision.body=b3CreateBody(collision.world,&bd);
    auto sd=b3DefaultShapeDef();const auto floor=b3MakeOffsetBoxHull(8,.25f,8,{0,-.25f,0});
    b3CreateHullShape(collision.body,&sd,&floor.base);
    const auto wall=b3MakeOffsetBoxHull(.15f,2,2,{1.2f,2,0});b3CreateHullShape(collision.body,&sd,&wall.base);
    {
      auto def=b3DefaultBodyDef();def.type=b3_dynamicBody;def.position={0,10,0};const auto decomposed=b3CreateBody(collision.world,&def);
      std::vector<b3Vec3> cube;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})cube.push_back({x*.5f,y*.5f,z*.5f});
      std::vector<b3HullData*> hulls;
      require(octaryn::character_motion::attach_decomposed_scene_hull(decomposed,hulls,cube,sd),"exact hull decomposition failed");
      require(std::abs(b3Body_GetMass(decomposed)/sd.density-1)<.0001f,"decomposition volume differs from source cube");
      for(const b3Vec3 direction:std::vector<b3Vec3>{{0,1,0},{1,0,0},{0,0,1},{1,.3f,.2f},{-.7f,.5f,1}}) {
        const auto start=b3Add(b3Vec3{0,10,0},b3MulSV(3,direction));
        const auto hit=b3World_CastRayClosest(collision.world,start,b3MulSV(-4,direction),b3DefaultQueryFilter());
        const float scale=.5f/std::max({std::abs(direction.x),std::abs(direction.y),std::abs(direction.z)});
        const auto expected=b3Add(b3Vec3{0,10,0},b3MulSV(scale,direction));
        require(hit.hit && b3Distance(hit.point,expected)<.0001f,"decomposition changes source boundary");
      }
      b3DestroyBody(decomposed);for(auto* hull:hulls)b3DestroyHull(hull);
    }
    SceneBodies bodies(&collision);
    octaryn_scene_body_shape shapes[4]{};
    for(auto& s:shapes)s.local_rotation[3]=1;
    shapes[0].kind=1;shapes[0].half_extents[0]=shapes[0].half_extents[1]=shapes[0].half_extents[2]=.2f;
    shapes[1].kind=2;shapes[1].radius=.2f;
    shapes[2].kind=3;shapes[2].radius=.1f;shapes[2].capsule_a[1]=-.15f;shapes[2].capsule_b[1]=.15f;
    const float points[]={-.2f,-.2f,-.2f,.2f,-.2f,-.2f,0,-.2f,.2f,0,.2f,0};
    shapes[3].kind=4;shapes[3].points=points;shapes[3].point_count=4;
    for(unsigned kind=0;kind<4;++kind) {
      auto d=desc(kind+1,&shapes[kind],1);d.position[0]=-float(kind);
      uint64_t handle{};require(bodies.create(d,handle)==0,"shape rejected");
      step(bodies,120);const auto p=pose(bodies,handle);
      require(p.position[1]>.05f && p.position[1]<.6f,"body did not contact floor");
      std::printf("scene_body_probe shape=%u contact_y=%.6f box3d=1\n",kind+1,p.position[1]);
      require(bodies.remove(handle)==0,"remove failed");octaryn_scene_body_pose dummy{};require(bodies.pose(handle,dummy)==1,"stale handle accepted");
    }
    shapes[1].local_position[0]=.25f;
    auto suspended_desc=desc(90,shapes,1);suspended_desc.position[0]=-2;uint64_t suspended{};
    require(bodies.create(suspended_desc,suspended)==0,"suspend body rejected");step(bodies,6);
    const auto frozen=pose(bodies,suspended);bodies.suspend(suspended,true);step(bodies,60);
    const auto paused=pose(bodies,suspended);
    require(paused.position[1]==frozen.position[1] && paused.velocity[1]==frozen.velocity[1] && (paused.flags&8)!=0,"suspension lost exact state");
    bodies.suspend(suspended,false);step(bodies,10);require(pose(bodies,suspended).position[1]<paused.position[1],"suspended body did not resume");bodies.remove(suspended);
    auto d=desc(100,shapes,2);d.position[1]=1;uint64_t handle{};
    require(bodies.create(d,handle)==0,"compound rejected");
    const float point[]={0,1,0},target[]={2,1,0};
    require(bodies.grab(handle,point,target,100)==0,"grab failed");step(bodies,120);
    auto held=pose(bodies,handle);require(held.position[0]<1.05f,"grab crossed wall");require((held.flags&2)!=0,"grab flag missing");
    require(std::hypot(held.rotation[0],held.rotation[1],held.rotation[2])>.01f,"compound never rotated against wall");
    std::printf("scene_body_probe compound_x=%.6f compound_rotation=%.6f,%.6f,%.6f,%.6f\n",held.position[0],held.rotation[0],held.rotation[1],held.rotation[2],held.rotation[3]);
    const float origin[]={3,1,0},direction[]={-1,0,0};octaryn_scene_body_hit hit{};
    require(bodies.ray(origin,direction,5,hit)==1,"ray selected through wall");
    bodies.release();step(bodies,120);auto released=pose(bodies,handle);
    require((released.flags&2)==0 && released.position[1]<held.position[1]-.1f,"release did not restore gravity");
    require(bodies.remove(handle)==0,"compound remove failed");
    auto cup_desc=desc(101,shapes,1);cup_desc.mass=.3f;cup_desc.position[0]=-.5f;cup_desc.position[1]=.21f;
    uint64_t cup{};require(bodies.create(cup_desc,cup)==0,"cup rejected");
    octaryn::character_motion::State actor{};actor.x=-2;actor.y=1.62f;actor.is_on_ground=1;
    octaryn::character_motion::Input input{};input.move_x=1;
    for(unsigned frame=0;frame<180;++frame) {
      octaryn::character_motion::move_walk_on_mesh(input,1.f/60,actor,0,0,&collision);step(bodies,1);
    }
    const auto pushed=pose(bodies,cup);require(pushed.position[0]>-.3f,"walking did not push cup");
    require(actor.x<.8f,"character pushed through fixed wall");
    std::printf("scene_body_probe character_push_cup_x=%.6f actor_wall_x=%.6f\n",pushed.position[0],actor.x);
    bodies.remove(cup);
    require(bodies.step(NAN)<0 && bodies.step(10)<0,"unbounded step accepted");
    std::puts("scene_body_probe result=pass gravity=1 compound=1 rotation=1 grab_collision=1 occlusion=1 release=1 removal=1 suspension=1 character_push=1 fixed_wall=1 decomposition_boundary=1 decomposition_volume=1");return 0;
  }catch(const std::exception& e) {std::fprintf(stderr,"scene_body_probe result=fail reason=%s\n",e.what());return 1;}
}
