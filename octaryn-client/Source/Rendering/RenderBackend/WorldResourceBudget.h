#pragma once
#include <slang-rhi.h>
#include <cstdint>
#include <stdexcept>

namespace octaryn::client::rendering {
inline bool world_resource_budget_allows(const rhi::ResourceRetirementInfo& info,bool& paused) {
  if(!info.asynchronous) {paused=false;return true;}
  constexpr std::uint64_t MiB=1024ull*1024;
  paused=paused?(info.pendingCount>256 || info.pendingBufferBytes>64*MiB):
      (info.pendingCount>=512 || info.pendingBufferBytes>=128*MiB);
  return !paused;
}
inline bool world_resource_budget_allows(rhi::ICommandQueue* queue,bool& paused,
    rhi::ResourceRetirementInfo* observed=nullptr) {
  rhi::ResourceRetirementInfo info{};
  if(!queue || SLANG_FAILED(queue->getResourceRetirementInfo(&info)))
    throw std::runtime_error("Resource retirement query failed");
  if(observed)*observed=info;
  return world_resource_budget_allows(info,paused);
}
}
