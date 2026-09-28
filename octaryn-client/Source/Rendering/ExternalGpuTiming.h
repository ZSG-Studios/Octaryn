#pragma once
#include <slang-rhi.h>
#include <cstdint>
#include "../MapWorld/MapRaySubmitScope.h"
namespace octaryn::client::rendering {
// One independently submitted AS operation precedes this frame on the same queue.
struct ExternalGpuTiming {
  bool started{},ended{};
  MapRaySubmitKind kind{};
  void reset() {*this={};}
  bool begin(rhi::ICommandEncoder* commands,rhi::IQueryPool* queries,unsigned first,MapRaySubmitKind value) {
    if(started || !commands || !queries)return false;
    started=true;kind=value;commands->writeTimestamp(queries,first);return true;
  }
  bool end(rhi::ICommandEncoder* commands,rhi::IQueryPool* queries,unsigned first) {
    if(!started || ended || !commands || !queries)return false;
    commands->writeTimestamp(queries,first+1);ended=true;return true;
  }
  bool resolve(rhi::IQueryPool* queries,unsigned first,std::uint64_t main_begin,
      double milliseconds_per_tick,double& milliseconds) const {
    milliseconds=0;if(!started)return true;
    if(!ended)return false;
    // Resolve main timestamps first: their submission follows this operation.
    // Refuse an incomplete external range instead of introducing another wait.
    rhi::QueryResultState state{};
    if(SLANG_FAILED(queries->getResultState(first,2,&state)) || state!=rhi::QueryResultState::Resolved)return false;
    std::uint64_t ticks[2]{};
    if(SLANG_FAILED(queries->getResult(first,2,ticks)) || ticks[1]<ticks[0] || ticks[1]>main_begin)return false;
    milliseconds=double(ticks[1]-ticks[0])*milliseconds_per_tick;return true;
  }
};
}
