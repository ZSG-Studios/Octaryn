#include "octaryn-client/Source/Rendering/Performance/PerformanceProfile.h"
#include "octaryn-client/Source/Rendering/Temporal/TemporalResolution.h"
#include "octaryn-client/Source/Diagnostics/AsyncProfileStream.h"
#include <cassert>
#include <fstream>
#include <limits>
#include <sstream>
#include <iostream>

int main(int argc,char** argv) {
  assert(argc==2);
  using namespace octaryn::client::rendering;
  WorldSceneSettings input;input.ray_tracing=false;input.clouds=false;
  const auto profile=apply_performance_profile(PerformanceProfile::HQ200,input);
  assert(profile.ray_tracing && profile.pbr && !profile.clouds);
  assert(profile.upscaler_mode==6 && profile.fsr_gpu_budget_ms==4.4f);
  assert(profile.fsr_min_scale==.5f && profile.fsr_max_scale==.75f);
  assert(!apply_performance_profile(PerformanceProfile::Custom,input).ray_tracing);
  TemporalResolution controller;
  controller.configure(6,2.f/3.f,true,.5f,.75f,200,4.4f);
  assert(std::round(2560*controller.scale)==1707 && std::round(1440*controller.scale)==960);
  for(unsigned i=0;i<1000;++i)controller.sample(20);
  assert(controller.scale==.5f && controller.floor_overruns>0);
  const auto samples=controller.samples;
  assert(!controller.sample(std::numeric_limits<float>::quiet_NaN()));
  assert(!controller.sample(0) && controller.samples==samples);
  for(unsigned i=0;i<1000;++i)controller.sample(1);
  assert(controller.scale==.75f);
  controller.configure(6,2.f/3.f,true,.5f,.75f,200,4.4f);
  for(unsigned i=0;i<100;++i)assert(!controller.sample(4.4f));
  assert(std::abs(controller.scale-2.f/3.f)<.0001f);
  const std::filesystem::path path(argv[1]);
  octaryn::client::diagnostics::AsyncProfileStream output;
  output.open(path);assert(output && output.is_open());
  std::ostringstream expected;
  for(unsigned i=0;i<10000;++i) {output<<i<<",4.4,720\n";expected<<i<<",4.4,720\n";}
  assert(output.close());
  std::ifstream stored(path,std::ios::binary);
  const std::string actual((std::istreambuf_iterator<char>(stored)),{});
  assert(actual==expected.str());
  octaryn::client::diagnostics::AsyncProfileStream invalid;
  invalid.open(path/"missing");assert(!invalid);
  std::cout<<"performance_profile_probe=passed resolution_floor=720 ordered_rows=10000\n";
}
