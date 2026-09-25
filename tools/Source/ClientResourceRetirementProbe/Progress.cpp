#include "Probe.h"
#include "../../../octaryn-client/Source/Rendering/RenderBackend/WorldRetirementProgress.h"
#include <cstdio>

namespace retirement_probe {
namespace {
using octaryn::client::rendering::WorldRetirementGuard;
using octaryn::client::rendering::WorldRetirementProgress;
constexpr std::uint64_t Timeout=100;

void stalled_cleanup() {
  WorldRetirementGuard guard;
  const WorldRetirementProgress work{0,1,64};
  require(guard.observe(work,0,Timeout),"first cleanup observation did not establish a baseline");
  for(std::uint64_t now=1;now<Timeout;++now)
    require(guard.observe(work,now,Timeout),"cleanup guard rejected a wait inside its timeout");
  require(!guard.observe(work,Timeout,Timeout),
      "repeated healthy closing observations hid a stalled resource destructor");
  require(!guard.observe(work,Timeout+1,Timeout),"continued stalled cleanup regained its deadline");
}

void independent_progress() {
  for(unsigned field=0;field<3;++field) {
    WorldRetirementGuard guard;
    WorldRetirementProgress work{6,6,600};
    require(guard.observe(work,1000,Timeout),"nonzero-clock cleanup baseline failed");
    if(field==0)--work.owners;
    if(field==1)--work.pending_count;
    if(field==2)--work.pending_buffer_bytes;
    require(guard.observe(work,1099,Timeout),"real cleanup progress was rejected");
    require(guard.observe(work,1198,Timeout),"real cleanup progress did not extend the deadline");
    require(!guard.observe(work,1199,Timeout),"progress deadline failed at the exact timeout");
  }
}

void growth_is_not_progress() {
  WorldRetirementGuard guard;
  require(guard.observe({1,1,10},1000,Timeout),"growth baseline failed");
  require(guard.observe({2,2,20},1099,Timeout),"increased work was rejected before timeout");
  require(!guard.observe({3,3,30},1100,Timeout),"increased retained work reset a stalled cleanup deadline");

  WorldRetirementGuard decreasing;
  require(decreasing.observe({1,1,10},1000,Timeout),"decrease baseline failed");
  require(decreasing.observe({3,3,30},1090,Timeout),"increased pending work failed before timeout");
  require(decreasing.observe({2,2,20},1099,Timeout),"decrease from the latest snapshot was ignored");
  require(decreasing.observe({2,2,20},1198,Timeout),"latest actual decrease did not extend deadline");
  require(!decreasing.observe({2,2,20},1199,Timeout),"latest actual decrease granted an unbounded wait");
}

void retained_owner_accounting() {
  require(WorldRetirementProgress{9,0,0}.remaining()==9,"logical owners missing from cleanup count");
  require(WorldRetirementProgress{0,1,0}.remaining()==1,"pending descriptor-only destruction hidden");
  require(WorldRetirementProgress{2,8,64}.remaining()==2,"pending resources changed logical batch accounting");
  require(WorldRetirementProgress{0,0,0}.remaining()==0,"finished cleanup never reaches zero");

  WorldRetirementGuard guard;
  require(guard.observe({1,1,64},1000,Timeout),"owner transition baseline failed");
  require(guard.observe({0,1,64},1099,Timeout),"last logical owner release was ignored");
  require(guard.observe({0,1,64},1198,Timeout),"pending native destructor was rejected too early");
  require(!guard.observe({0,1,64},1199,Timeout),"zero logical owners hid stalled native destruction");
}
}

void progress_cases() {
  stalled_cleanup();independent_progress();growth_is_not_progress();retained_owner_accounting();
  std::puts("resource_retirement_progress=passed fake_clock=1 exact_timeout=1 frame_activity_not_progress=1 "
      "owners_count_bytes=1 growth_not_progress=1 pending_native_cleanup=1");
}
}
