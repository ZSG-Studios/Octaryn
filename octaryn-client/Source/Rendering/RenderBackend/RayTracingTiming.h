#pragma once
#include <slang-rhi.h>
#include <array>
namespace octaryn::client::rendering {
// Resolve only after this work's submission fence, before reusing its query pool.
class RayTracingTiming {
  Slang::ComPtr<rhi::IQueryPool> queries_;
  double scale_{};
  bool pending_{};
public:
  bool begin(rhi::IDevice* device,rhi::ICommandEncoder* commands,bool supported) {
    if(!supported || !device->getInfo().timestampFrequency)return true;
    if(pending_)return false;
    if(!queries_) {
      rhi::QueryPoolDesc desc{};desc.type=rhi::QueryType::Timestamp;desc.count=2;
      if(SLANG_FAILED(device->createQueryPool(desc,queries_.writeRef())))return false;
      scale_=1000.0/static_cast<double>(device->getInfo().timestampFrequency);
    }
    if(SLANG_FAILED(queries_->reset()))return false;
    commands->writeTimestamp(queries_,0);return true;
  }
  void end(rhi::ICommandEncoder* commands) {
    if(!queries_)return;
    commands->writeTimestamp(queries_,1);pending_=true;
  }
  bool resolve(double& milliseconds) {
    if(!pending_)return true;
    std::array<std::uint64_t,2> ticks{};
    if(SLANG_FAILED(queries_->getResult(0,2,ticks.data())) || ticks[1]<ticks[0])return false;
    milliseconds=static_cast<double>(ticks[1]-ticks[0])*scale_;
    pending_=false;return true;
  }
};
}
