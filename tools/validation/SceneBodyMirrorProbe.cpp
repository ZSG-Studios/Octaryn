#include "SceneBodyMirror.h"
#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include "MeshCharacterStep.h"
#include <box3d/collision.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool condition,const char* reason) {if(!condition)throw std::runtime_error(reason);}
}
int main(int argc,char** argv) {
  try {
    using namespace octaryn;
    auto scene=std::make_shared<character_motion::MeshCollisionScene>();
    auto* collision=scene->collision_world();
    auto bd=b3DefaultBodyDef();const auto floor_body=b3CreateBody(collision->world,&bd);
    auto sd=b3DefaultShapeDef();const auto floor=b3MakeOffsetBoxHull(20,.25f,20,{0,-.25f,0});
    b3CreateHullShape(floor_body,&sd,&floor.base);
    const auto fixture=std::filesystem::path("build/release-windows/tools/scene-body-probe/mirror.physics.json");
    std::ofstream(fixture)<<R"({"version":1,"bodies":[{"sourceId":1,"position":[0,0.5,0],"rotation":[0,0,0,1],"shapes":[{"kind":1,"localPosition":[0,0,0],"localRotation":[0,0,0,1],"halfExtents":[0.25,0.5,0.25]}]}]})";
    client::app::SceneBodyMirror mirror;
    const auto source=fixture.parent_path()/"mirror.gltf";
    require(mirror.load(scene,source),"mirror fixture failed admission");
    auto ray=[&](float x) {return b3World_CastRayClosest(collision->world,{x,4,0},{0,-3,0},b3DefaultQueryFilter()).hit;};
    require(ray(0),"initial source shape missing");
    const float position[]={2,.5f,0},rotation[]={0,0,0,1},velocity[]={0,0,0};
    require(mirror.pose(1,position,rotation,velocity,false),"authority pose rejected");
    require(!ray(0) && ray(2),"mirror did not follow authority");
    character_motion::State actor{};actor.x=0;actor.y=1.62f;actor.is_on_ground=1;
    character_motion::Input input{};input.move_x=1;
    for(unsigned i=0;i<60;++i)character_motion::move_walk_on_mesh(input,1.f/60,actor,0,0,collision);
    require(actor.x<1.5f && actor.x>1.4f,"prediction did not block at moved object");
    require(mirror.pose(1,position,rotation,velocity,true) && !ray(2),"removed mirror still collides");
    for(unsigned i=0;i<30;++i)character_motion::move_walk_on_mesh(input,1.f/60,actor,0,0,collision);
    require(actor.x>2,"removed object still blocks prediction");
    std::puts("scene_body_mirror_probe fixture=pass source_shape=1 authoritative_pose=1 prediction_contact=1 removal=1 client_authority=0");
    for(int i=1;i<argc;++i) {
      auto owned=std::make_shared<character_motion::MeshCollisionScene>();client::app::SceneBodyMirror loader;
      require(loader.load(owned,std::filesystem::u8path(argv[i])),"owned source mirror failed admission");
      std::printf("scene_body_mirror_probe owned=%s admitted=1\n",argv[i]);
    }
    return 0;
  }catch(const std::exception& e) {std::fprintf(stderr,"scene_body_mirror_probe result=fail reason=%s\n",e.what());return 1;}
}
