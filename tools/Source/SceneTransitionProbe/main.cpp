#include "MapSceneTransition.h"
#include <iostream>
#include <limits>

namespace fs=std::filesystem;
using namespace octaryn::client::app;
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("Expected new fixture directory");
    const auto root=fs::absolute(argv[1]);
    if(fs::exists(root))throw std::runtime_error("Preserve previous fixture evidence");
    fs::create_directories(root/"package");
    const auto descriptor=root/"package/scene-import.json";
    const auto write=[&](const std::string& text) {std::ofstream file(descriptor);file<<text;};
    std::ofstream(root/"package/scene.gltf")<<"{}";
    octaryn::client::host::SceneTransitionRequest request{17,"fixture.scene",descriptor,{4,5,6,.2f,-.1f},0};
    write(R"({"version":1,"prepared":true,"scene":"scene.gltf","source":{"path":"retained"}})");
    const auto loaded=resolve_scene_transition(request,root/"runtime");
    unsigned checks=0;
    const auto require=[&](bool value) {if(!value)throw std::runtime_error("Transition assertion failed");++checks;};
    require(loaded.glb==fs::canonical(root/"package/scene.gltf"));
    require(loaded.scene_descriptor==fs::canonical(descriptor) && loaded.replace_scene);
    require(loaded.scene_asset=="fixture.scene" && loaded.spawn_y==5 && loaded.yaw==.2f);
    const auto read=[](const fs::path& path) {std::ifstream file(path);return std::string(std::istreambuf_iterator<char>(file),{});};
    TransitionServerManifest server;
    require(!glz::read_json(server,read(loaded.manifest)) && server.spawn[0]==4 && server.spawn[2]==6);
    auto previous=loaded;previous.spawn_x=-9;previous.spawn_y=7;
    write_scene_spawn_manifest(previous,root/"runtime","17-restore");
    require(previous.manifest!=loaded.manifest && !glz::read_json(server,read(previous.manifest)) && server.spawn[0]==-9 && server.spawn[1]==7);
    const auto denied=[&] {bool refused=false;try{resolve_scene_transition(request,root/"runtime");}catch(...){refused=true;}require(refused);};
    for(const auto* payload:{"../scene.gltf","C:/escape.gltf","folder/../scene.gltf","./scene.gltf","missing.gltf","scene.bin"}) {
      write("{\"version\":1,\"prepared\":true,\"scene\":\""+std::string(payload)+"\"}");denied();
    }
    for(const auto* invalid:{"{}","{\"version\":2,\"prepared\":true,\"scene\":\"scene.gltf\"}","{\"version\":1,\"prepared\":false,\"scene\":\"scene.gltf\"}"}) {write(invalid);denied();}
    write(std::string(4*1024*1024+1,' '));denied();
    write(R"({"version":1,"prepared":true,"scene":"scene.gltf"})");
    request.revision=0;denied();request.revision=17;request.pose.x=std::numeric_limits<double>::quiet_NaN();denied();
    std::cout<<"scene_transition_probe checks="<<checks<<" passed=1 gpu=0\n";return 0;
  }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
