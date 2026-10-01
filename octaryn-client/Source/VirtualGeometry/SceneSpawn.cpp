#include "ScenePreparation.h"
#include "SceneCollisionResidency.h"
#include "CharacterGeometry.h"
#include "CharacterCollision.h"
#include "MeshCollisionWorld.h"
#include <chrono>
#include <cmath>
#include <thread>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
bool clear_capsule(b3WorldId world,const character_motion::State& state) {
  using namespace character_motion;
  const b3Capsule capsule{{0,CollisionRadius,0},{0,CollisionHeight-CollisionRadius,0},CollisionRadius};
  bool clear=true;
  b3World_CollideMover(world,{state.x,state.y-EyeOffset,state.z},&capsule,b3DefaultQueryFilter(),
      [](b3ShapeId,const b3PlaneResult* planes,int count,void* value) {
        for(int i=0;i<count;++i)if(planes[i].plane.offset>.0051f) {*static_cast<bool*>(value)=false;return false;}
        return true;
      },&clear);
  return clear;
}
}
bool qualify_scene_spawn(const std::filesystem::path& catalog,const std::filesystem::path& source,
    const std::array<float,3>& hint,std::array<float,3>& output,std::string& error,const std::atomic_bool* cancel) {
  try {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    const auto check=[&] {
      if(cancel && cancel->load())throw std::runtime_error("scene spawn qualification canceled");
      if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("scene spawn qualification exceeded 60 seconds");
    };
    check();for(float value:hint)if(!std::isfinite(value))throw std::runtime_error("scene spawn hint is not finite");
    character_motion::SceneCollisionResidency residency;
    if(!residency.load(catalog,source,catalog.parent_path()/"spawn-scratch",512ull<<20,cancel))throw std::runtime_error(residency.error());
    const auto collision=residency.scene()->view();auto* world=character_motion::acquire_mesh_world(collision);
    if(!world)throw std::runtime_error("scene spawn collision world is empty");
    for(unsigned ring=0;ring<=16;++ring)for(unsigned sample=0;sample<(ring?8*ring:1);++sample) {
      check();const float angle=ring?float(sample)*6.28318530718f/float(8*ring):0;
      const float x=hint[0]+float(ring)*.5f*std::cos(angle),z=hint[2]+float(ring)*.5f*std::sin(angle);
      const std::array<float,6> column{x-.01f,hint[1]-30.01f,z-.01f,x+.01f,hint[1]+2.01f,z+.01f};
      while(!residency.ready_bounds(column)) {
        check();if(!residency.error().empty())throw std::runtime_error(residency.error());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      const auto floor=b3World_CastRayClosest(world->world,{x,hint[1]+2,z},{0,-32,0},b3DefaultQueryFilter());
      if(!floor.hit || floor.normal.y<.70710678f)continue;
      character_motion::State state{};state.x=x;state.y=float(floor.point.y)+character_motion::EyeOffset+.02f;state.z=z;
      const auto ready=[&](float radius) {
        while(!residency.ready(state.x,state.y,state.z,radius)) {
          check();if(!residency.error().empty())throw std::runtime_error(residency.error());
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      };
      ready(character_motion::CharacterCollisionRadius);
      if(!clear_capsule(world->world,state))continue;
      character_motion::Input input{};
      for(unsigned tick=0;tick<120;++tick) {
        check();ready(character_motion::character_collision_radius(state,input,1.f/60));
        character_motion::step_on_mesh(input,1.f/60,state,collision);
      }
      if(!state.is_on_ground || std::abs(state.x-x)>.1f || std::abs(state.z-z)>.1f ||
          std::abs(state.y-float(floor.point.y)-character_motion::EyeOffset)>.05f || !clear_capsule(world->world,state))continue;
      output={state.x,state.y,state.z};error.clear();return true;
    }
    throw std::runtime_error("no clear grounded player capsule within eight meters of the scene view hint");
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
