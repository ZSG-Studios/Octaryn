#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace octaryn::client::rendering {
enum LightingResource : std::uint32_t {
  SurfaceResource=1,RaySceneResource=2,ProbeResource=4,LocalResource=8,ShadowResource=16,
  SceneResource=32,LightResource=64,VoxelTraceResource=128,SrcResource=256
};
// Small dependency scheduler over retained resources; RHI owns concrete barriers.
class LightingGraph {
  struct Pass {std::uint32_t reads{},writes{};std::function<bool()> execute;bool done{};};
  std::array<Pass,8> passes_;unsigned count_{};
public:
  bool add(std::uint32_t reads,std::uint32_t writes,std::function<bool()> execute) {
    if(count_==passes_.size())return false;
    passes_[count_++]={reads,writes,std::move(execute),false};return true;
  }
  bool execute(std::uint32_t available) {
    for(unsigned completed=0;completed<count_;) {
      bool progress=false;
      for(unsigned i=0;i<count_;++i) {
        auto& p=passes_[i];if(p.done || (p.reads&available)!=p.reads)continue;
        if(!p.execute())return false;p.done=true;available|=p.writes;++completed;progress=true;
      }
      if(!progress)return false;
    }
    return true;
  }
};
}
