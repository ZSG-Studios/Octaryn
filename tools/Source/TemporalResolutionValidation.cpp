#include "../../octaryn-client/Source/Rendering/Temporal/TemporalResolution.h"
#include "../../octaryn-client/Source/Rendering/Temporal/TemporalCamera.h"
#include <cstdio>
#include <limits>
using namespace octaryn::client::rendering;
namespace {
unsigned checks{},failures{};
void check(bool result,const char* name) {
  ++checks;if(!result) {++failures;std::fprintf(stderr,"temporal_resolution_failed=%s\n",name);}
}
}
int main() {
  TemporalResolution r;
  r.configure(1,.5f,true,.4f,.8f,60);
  check(!r.active && r.scale==1,"native_ignores_drs_and_scale");
  r.configure(0,.5f,true,.4f,.8f,60);
  check(!r.active && r.scale==1,"off_ignores_drs_and_scale");
  r.configure(6,.72f,false,.5f,1,60);
  check(!r.active && std::abs(r.scale-.72f)<1e-6,"custom_exact_scale");
  r.configure(2,.72f,true,.5f,.8f,60);
  check(r.active && std::abs(r.scale-2.f/3)<1e-6,"quality_initial_scale");
  const float initial=r.scale;
  check(!r.sample(std::numeric_limits<float>::quiet_NaN()) && !r.sample(-1) && !r.sample(0) &&
      !r.sample(1001) && r.samples==0 && r.scale==initial,"invalid_gpu_samples_ignored");
  for(unsigned i=0;i<500;++i)r.sample(32);
  check(r.scale==.5f,"over_budget_reaches_minimum");
  for(unsigned i=0;i<500;++i)r.sample(4);
  check(r.scale==.8f,"under_budget_reaches_maximum");
  r.configure(2,.72f,true,.5f,1,60);
  for(unsigned i=0;i<200;++i)r.sample(1000.f/60*.95f);
  check(r.scale==initial,"deadband_prevents_oscillation");
  r.configure(6,.7f,true,.8f,.4f,999);
  check(r.scale==.8f && r.minimum==.8f && r.maximum==.8f && r.target_fps==240,"bounds_and_target_sanitized");
  r.configure(6,-3,false,.5f,1,1);
  check(r.scale==1.f/3 && r.target_fps==30,"custom_and_target_lower_bound");
  TemporalCamera history;WorldCamera camera{};
  history.commit(camera,1280,720);
  check(history.reset(camera,1200,675,.016),"fixed_resolution_change_resets");
  check(!history.reset(camera,1200,675,.016,true),"dynamic_resolution_preserves_history");
  camera.x=100;
  check(history.reset(camera,1200,675,.016,true),"camera_cut_still_resets_dynamic");
  std::printf("temporal_resolution_checks=%u failures=%u\n",checks,failures);
  return failures?1:0;
}
