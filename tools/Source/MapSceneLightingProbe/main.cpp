#include "../../../octaryn-client/Source/MapWorld/MapModel.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
unsigned assertions{};
void check(bool value,const char* message) {++assertions;if(!value)throw std::runtime_error(message);}
bool near(float a,float b) {return std::abs(a-b)<.0001f;}
int main(int argc,char** argv) {
  try {
    check(argc==2,"fixture directory required");const auto root=std::filesystem::path(argv[1]);std::string error;
    MapModel plain;check(load_map_model(root/"plain.gltf",plain,error),error.c_str());
    check(!plain.environment.enabled && plain.environment.sky_enabled && plain.lights.empty(),"ordinary scene changed");
    MapModel lit;check(load_map_model(root/"valid.gltf",lit,error),error.c_str());
    check(lit.environment.enabled && !lit.environment.sky_enabled && lit.lights.size()==8,"authored lighting absent");
    check(near(lit.environment.ambient[0],.1f) && near(lit.environment.directional_color[1],.5f),"environment color changed");
    check(lit.environment.directional_direction==std::array<float,3>{0,-1,0},"directional light vector changed");
    for(unsigned i=0;i<8;++i) {
      const auto& light=lit.lights[i];
      check(near(light.position_range[0],10) && near(light.position_range[1],20) && near(light.position_range[2],30-2.f*i),"parent light transform lost");
      check(near(light.position_range[3],3.f+i),"light range changed under node scale");
      check(near(light.color_intensity[0],.2f) && near(light.color_intensity[1],.3f) && near(light.color_intensity[2],.4f) && near(light.color_intensity[3],4.f+i),"light color/intensity lost");
    }
    for(const auto* name:{"version","length","numeric","negative","huge","direction","zero","sky","unknown","missing","norange","rangezero","rangehuge","intensitynegative","intensityhuge","spot","colornegative","positionhuge","overflow","nonfinite"}) {
      MapModel retained;retained.environment.enabled=true;retained.environment.ambient[0]=99;
      const bool admitted=load_map_model(root/(std::string(name)+".gltf"),retained,error);
      if(admitted)std::fprintf(stderr,"unexpected lighting fixture=%s\n",name);
      check(!admitted,"unsupported lighting accepted");
      check(retained.environment.ambient[0]==99 && !error.empty(),"failed import published partial scene");
    }
    MapModel alternate;check(load_map_model(root/"alternate.gltf",alternate,error),error.c_str());
    check(!alternate.environment.enabled && alternate.lights.empty(),"unselected scene lighting leaked");
    std::printf("map_scene_lighting passed=1 assertions=%u gpu=0\n",assertions);return 0;
  }catch(const std::exception& error) {std::fprintf(stderr,"map_scene_lighting failed=%s\n",error.what());return 1;}
}
