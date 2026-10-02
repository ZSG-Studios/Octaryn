#include "../../../octaryn-client/Source/App/OpenWorld/ScenePreviewCamera.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using octaryn::client::app::ScenePreviewCamera;
struct Movement {bool move_forward{},move_backward{},move_left{},move_right{},move_up{},move_down{},sprint{};};
struct Controls {float yaw{},pitch{};Movement movement;};
struct Camera {double x{},y{},z{};float yaw{},pitch{},jitter_x{},jitter_y{},vertical_fov{};};
struct Input {
  bool forward{},backward{},left{},right{},up{},down{},sprint{},flying{},has_jump_events{};
  std::array<unsigned,3> jump_events{};
  float yaw{},pitch{};
};
unsigned checks{};
void check(bool condition,const char* message) {
  ++checks;if(!condition)throw std::runtime_error(message);
}
bool near(double actual,double expected) {return std::abs(actual-expected)<1e-6;}
double distance(const Camera& a,const Camera& b) {
  return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
}
void initial_and_disabled() {
  Camera camera{99,200,-100,1,1,.004f,-.005f,1.1f};Controls controls{2,.5f,{}};
  ScenePreviewCamera preview("1.25,0.5,-2.5,0.25,-0.12");
  check(preview.enabled(),"explicit preview was not enabled");
  preview.apply(camera,controls,0);
  check(near(camera.x,1.25)&&near(camera.y,.5)&&near(camera.z,-2.5),"player pose displaced authored origin");
  check(near(camera.yaw,.25)&&near(camera.pitch,-.12),"authored look was not initialized");
  check(camera.jitter_x==.004f&&camera.jitter_y==-.005f&&camera.vertical_fov==1.1f,"projection/jitter was modified");
  controls.yaw=.7f;controls.pitch=.3f;camera.x=500;preview.apply(camera,controls,0);
  check(near(camera.x,1.25)&&near(camera.yaw,.7)&&near(camera.pitch,.3),"subsequent pose/look handling failed");
  for(const char* request:{static_cast<const char*>(nullptr),""}) {
    ScenePreviewCamera disabled(request);Controls unchanged{.3f,.4f,{}};Camera source{5,6,7,.8f,.9f,.02f,-.03f,1.2f};
    unchanged.movement.move_forward=true;disabled.apply(source,unchanged,1);
    check(!disabled.enabled()&&source.x==5&&source.y==6&&source.z==7&&source.yaw==.8f&&source.pitch==.9f,
        "disabled mode changed camera");
    check(unchanged.yaw==.3f&&unchanged.pitch==.4f&&source.jitter_x==.02f&&source.vertical_fov==1.2f,
        "disabled mode changed controls or projection");
  }
}
void motion() {
  ScenePreviewCamera preview("0,0,0,0,0");Controls controls;Camera camera;preview.apply(camera,controls,0);
  controls.movement.move_forward=true;preview.apply(camera,controls,.05);
  check(near(camera.z,-.05)&&near(camera.x,0)&&near(camera.y,0),"forward basis is wrong");
  controls.yaw=1.57079632679f;Camera before=camera;preview.apply(camera,controls,.05);
  check(near(camera.x-before.x,.05)&&near(camera.z,before.z),"yaw did not rotate movement");
  controls.yaw=0;controls.pitch=.5f;before=camera;preview.apply(camera,controls,.05);
  check(near(camera.y-before.y,std::sin(.5)*.05)&&near(camera.z-before.z,-std::cos(.5)*.05),"pitch did not rotate movement");
  controls.movement={};controls.movement.move_up=true;before=camera;preview.apply(camera,controls,.05);
  check(near(camera.y-before.y,.05)&&near(camera.x,before.x)&&near(camera.z,before.z),"vertical movement is not world-aligned");
  controls.movement.move_forward=true;controls.movement.move_right=true;before=camera;preview.apply(camera,controls,.05);
  check(near(distance(camera,before),.05),"pitched diagonal exceeded normalized speed");
  controls.pitch=0;before=camera;preview.apply(camera,controls,.05);
  check(near(distance(camera,before),.05),"diagonal exceeded normalized speed");
  controls.movement.sprint=true;before=camera;preview.apply(camera,controls,100);
  check(near(distance(camera,before),.4),"sprint or elapsed-time clamp failed");
  for(double dt:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
    before=camera;preview.apply(camera,controls,dt);check(near(distance(camera,before),0),"invalid elapsed time moved preview");
  }
  controls.movement={};controls.movement.move_forward=controls.movement.move_backward=true;
  before=camera;preview.apply(camera,controls,.1);check(near(distance(camera,before),0),"opposing movement did not cancel");
  ScenePreviewCamera bounded("1000000,1000000,1000000,0,0");controls.movement={};controls.movement.move_right=controls.movement.move_up=true;
  controls.movement.sprint=true;bounded.apply(camera,controls,.1);
  check(camera.x<=1000000&&camera.y<=1000000&&camera.z<=1000000,"position escaped admitted bounds");
}
void parsing_and_authority() {
  for(const char* request:{"1,2,3,4","1,2,3,4,5,6","nan,0,0,0,0","0,inf,0,0,0","1000001,0,0,0,0",
      "0,0,0,1000001,0","0,0,0,0,1.56","0,0,0,0,-1.56","0,0,0,0,0tail"}) {
    bool rejected=false;try {ScenePreviewCamera invalid(request);}catch(const std::runtime_error&) {rejected=true;}
    check(rejected,"malformed or unbounded origin admitted");
  }
  Input input{true,true,true,true,true,true,true,true,true,{1,2,3},.75f,-.25f};
  ScenePreviewCamera disabled(nullptr);disabled.suppress(input);
  check(input.forward&&input.backward&&input.left&&input.right&&input.up&&input.down&&input.sprint&&input.flying&&
      input.has_jump_events&&input.jump_events[2]==3,"disabled suppression changed authority inputs");
  ScenePreviewCamera preview("0,0.5,1.3,0,-0.12");preview.suppress(input);
  check(!input.forward&&!input.backward&&!input.left&&!input.right&&!input.up&&!input.down&&!input.sprint&&!input.flying,
      "preview leaked authoritative movement");
  check(!input.has_jump_events&&input.jump_events==std::array<unsigned,3>{},"preview leaked authoritative jump events");
  check(input.yaw==.75f&&input.pitch==-.25f,"movement suppression changed authority look contract");
}
}
int main() {
  try {initial_and_disabled();motion();parsing_and_authority();
    std::cout<<"scene_preview_camera_checks passed=1 assertions="<<checks<<" gpu=0\n";return 0;
  }catch(const std::exception& error) {std::cerr<<"scene_preview_camera_checks failed="<<error.what()<<'\n';return 1;}
}
