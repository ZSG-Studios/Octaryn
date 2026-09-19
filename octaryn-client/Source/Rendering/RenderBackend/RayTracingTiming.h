#pragma once
#include <slang-rhi.h>
#include <array>
#include "RayPrepareDiagnostics.h"
namespace octaryn::client::rendering {
// Resolve only after this work's submission fence, before reusing its query pool.
class RayTracingTiming {
  Slang::ComPtr<rhi::IQueryPool> queries_;
  double scale_{};
  bool pending_{};
public:
  bool begin(rhi::IDevice* device,rhi::ICommandEncoder* commands,bool supported,RayPrepareDiagnostics* diagnostic=nullptr) {
    const auto check=[&](const char* step,SlangResult result) {
      return diagnostic?diagnostic->check(step,result):SLANG_SUCCEEDED(result);
    };
    if(!supported || !device->getInfo().timestampFrequency)return true;
    if(!check("timing_not_pending",pending_?SLANG_FAIL:SLANG_OK))return false;
    if(!queries_) {
      rhi::QueryPoolDesc desc{};desc.type=rhi::QueryType::Timestamp;desc.count=2;
      if(!check("timing_create",device->createQueryPool(desc,queries_.writeRef())))return false;
      scale_=1000.0/static_cast<double>(device->getInfo().timestampFrequency);
    }
    if(!check("timing_reset",queries_->reset()))return false;
    commands->writeTimestamp(queries_,0);return true;
  }
  void end(rhi::ICommandEncoder* commands) {
    if(!queries_)return;
    commands->writeTimestamp(queries_,1);pending_=true;
  }
  bool resolve(double& milliseconds,RayPrepareDiagnostics* diagnostic=nullptr) {
    if(!pending_)return true;
    std::array<std::uint64_t,2> ticks{};
    const auto result=queries_->getResult(0,2,ticks.data());
    if(diagnostic) {
      if(!diagnostic->check("timing_results",result) ||
         !diagnostic->require("timing_order",ticks[1]>=ticks[0]))return false;
    } else if(SLANG_FAILED(result) || ticks[1]<ticks[0])return false;
    milliseconds=static_cast<double>(ticks[1]-ticks[0])*scale_;
    pending_=false;return true;
  }
};
}
