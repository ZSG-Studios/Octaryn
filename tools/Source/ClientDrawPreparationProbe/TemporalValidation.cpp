#include "TemporalCamera.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace octaryn::client::rendering;
void check_temporal_camera() {
  unsigned checks=0;
  auto expect=[&](bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);};
  TemporalCamera state;WorldCamera eye{100000,20,-100000,0,0,1.05f};
  expect(state.reset(eye,1280,720,.016),"first temporal frame resets");
  state.commit(eye,1280,720);
  expect(!state.reset(eye,1280,720,.016),"stationary submitted frame retains history");
  auto moved=eye;moved.x+=1;
  expect(!state.reset(moved,1280,720,.016),"ordinary motion retains history");
  expect(state.previous().x==eye.x,"unsubmitted candidate does not commit history");
  expect(state.reset(moved,1920,1080,.016),"render size change resets history");
  expect(state.reset(moved,1280,720,.5),"resume after long frame resets history");
  expect(state.reset(moved,1280,720,std::numeric_limits<double>::quiet_NaN()),"nonfinite delta resets history");
  moved.x+=100;expect(state.reset(moved,1280,720,.016),"teleport resets history");
  moved=eye;moved.yaw=6.283185307f;
  expect(!state.reset(moved,1280,720,.016),"wrapped equivalent yaw retains history");
  moved=eye;moved.vertical_fov+=.1f;
  expect(state.reset(moved,1280,720,.016),"projection discontinuity resets history");
  state.invalidate();expect(state.reset(eye,1280,720,.016),"resource replacement invalidates history");
  for(float yaw:{0.f,1.f,3.f})for(float pitch:{-.8f,0.f,.8f}) {
    eye.yaw=yaw;eye.pitch=pitch;
    const auto view=temporal_view(eye,1280,720);
    auto dot=[](const auto& a,const auto& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    expect(std::abs(dot(view.right,view.up))<1e-6 && std::abs(dot(view.forward,view.up))<1e-6 &&
        std::abs(dot(view.right,view.forward))<1e-6,"temporal camera basis orthogonal");
    expect(std::abs(dot(view.forward,view.forward)-1)<1e-6,"temporal forward normalized");
    expect(std::abs((view.projection[2]*.1f-view.projection[3])/.1f)<1e-5,"near plane maps to zero depth");
    expect(std::abs((view.projection[2]*8192-view.projection[3])/8192-1)<1e-6,"far plane maps to one depth");
  }
  std::printf("temporal_camera=passed checks=%u gpu_devices=0\n",checks);
}
