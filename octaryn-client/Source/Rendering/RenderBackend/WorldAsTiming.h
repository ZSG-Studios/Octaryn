#pragma once
#include "WorldGpuProfile.h"
#include "../Temporal/TemporalTiming.h"
namespace octaryn::client::rendering {
// The optional CSV profiler and production DRS feedback observe the same work.
struct WorldAsTiming {
  WorldGpuProfile* profile{};
  TemporalTiming* temporal{};
  unsigned slot{};
  bool started{},ended{};
  MapRaySubmitScope scope() {
    return {this,[](void* context,rhi::ICommandEncoder* commands,MapRaySubmitKind kind) {
      auto& self=*static_cast<WorldAsTiming*>(context);
      if(self.started)return false;
      self.started=true;
      return (!self.profile || self.profile->begin_external(commands,kind)) &&
          (!self.temporal || self.temporal->begin_external(commands,self.slot,kind));
    },[](void* context,rhi::ICommandEncoder* commands) {
      auto& self=*static_cast<WorldAsTiming*>(context);
      if(!self.started || self.ended)return false;
      self.ended=true;
      return (!self.profile || self.profile->end_external(commands)) &&
          (!self.temporal || self.temporal->end_external(commands,self.slot));
    }};
  }
};
}
