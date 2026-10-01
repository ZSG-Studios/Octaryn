#include "../../../octaryn-client/Source/Rendering/Temporal/TemporalTiming.h"
#include "../../../octaryn-client/Source/Rendering/RenderBackend/WorldAsTiming.h"
#include "../../../octaryn-client/Source/Rendering/RenderBackend/FrameSubmissionGuard.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
struct QueryPool final : rhi::IQueryPool {
  rhi::QueryPoolDesc desc{};
  std::uint64_t values[4]{10000,14000,8000,10000};
  rhi::QueryResultState external_state{rhi::QueryResultState::Resolved};
  unsigned main_reads{},external_reads{};
  bool fail_read{};
  SlangResult SLANG_MCALL queryInterface(const SlangUUID&,void**) noexcept override {return SLANG_E_NO_INTERFACE;}
  std::uint32_t SLANG_MCALL addRef() noexcept override {return 1;}
  std::uint32_t SLANG_MCALL release() noexcept override {return 1;}
  const rhi::QueryPoolDesc& SLANG_MCALL getDesc() noexcept override {return desc;}
  rhi::Result SLANG_MCALL getResultState(std::uint32_t first,std::uint32_t count,rhi::QueryResultState* state) noexcept override {
    if(count!=2 || first>2 || !state)return SLANG_E_INVALID_ARG;
    *state=first==2?external_state:rhi::QueryResultState::Resolved;return SLANG_OK;
  }
  rhi::Result SLANG_MCALL getResult(std::uint32_t first,std::uint32_t count,std::uint64_t* result) noexcept override {
    if(count!=2 || first>2 || !result || fail_read)return SLANG_FAIL;
    if(first==2)++external_reads;else ++main_reads;
    result[0]=values[first];result[1]=values[first+1];return SLANG_OK;
  }
  rhi::Result SLANG_MCALL reset() noexcept override {return SLANG_OK;}
  rhi::Result SLANG_MCALL reset(std::uint32_t,std::uint32_t) noexcept override {return SLANG_OK;}
};
}
int main() try {
  QueryPool query;TemporalTiming timing;timing.milliseconds_per_tick=.001;
  auto& slot=timing.slots[0];slot.queries=&query;
  const auto ready=[&](bool external=true) {
    slot.pending=true;slot.external={external,external,external?MapRaySubmitKind::Build:MapRaySubmitKind{}};
    query.values[0]=10000;query.values[1]=14000;query.values[2]=8000;query.values[3]=10000;
    query.external_state=rhi::QueryResultState::Resolved;query.fail_read=false;
  };
  float milliseconds{};
  ready();require(timing.resolve(0,milliseconds) && milliseconds==6,"DRS omitted external AS work");
  require(!slot.pending && query.main_reads==1 && query.external_reads==1,"resolve did not retire one frame");
  ready(false);require(timing.resolve(0,milliseconds) && milliseconds==4,"static frame changed GPU scope");
  require(query.external_reads==1,"unused external queries were read");
  ready();query.values[3]=10001;
  require(!timing.resolve(0,milliseconds),"overlapping/main-later AS accepted and double counted");
  ready();query.values[3]=7999;
  require(!timing.resolve(0,milliseconds),"descending external timestamps accepted");
  ready();query.values[1]=9999;
  require(!timing.resolve(0,milliseconds),"descending main timestamps accepted");
  for(auto state:{rhi::QueryResultState::Pending,rhi::QueryResultState::Reset}) {
    ready();query.external_state=state;const auto reads=query.external_reads;
    require(!timing.resolve(0,milliseconds) && query.external_reads==reads,
        "unresolved AS query invoked potentially blocking getResult");
  }
  ready();slot.external.ended=false;
  require(!timing.resolve(0,milliseconds),"unclosed AS span accepted");
  ready();query.fail_read=true;require(!timing.resolve(0,milliseconds),"failed query read accepted");
  ready();slot.external.kind=MapRaySubmitKind::Compaction;
  require(timing.resolve(0,milliseconds) && milliseconds==6,"compaction absent from production DRS");
  timing.reset();require(!slot.pending,"resize reset left pending timing");
  WorldAsTiming unprofiled;auto scope=unprofiled.scope();
  require(!scope.end(nullptr),"unstarted external operation ended");
  require(scope.begin(nullptr,MapRaySubmitKind::Build),"unprofiled operation refused");
  require(!scope.begin(nullptr,MapRaySubmitKind::Compaction),"unprofiled second operation exceeded quota");
  require(scope.end(nullptr) && !scope.end(nullptr),"external operation ended twice");
  unsigned retire_count{},failed_count{};bool external_started{};
  auto retire=[&] {++retire_count;return true;};
  auto failed=[&] {++failed_count;};
  {FrameSubmissionGuard guard(external_started,retire,failed);}
  require(retire_count==0,"empty frame added a queue wait");
  // Begin is observed by reference after guard construction, including a
  // partially recorded external operation that exits before acquire/submit.
  {FrameSubmissionGuard guard(external_started,retire,failed);external_started=true;}
  require(retire_count==1 && failed_count==0,"failed acquire left external queries unowned");
  {FrameSubmissionGuard guard(external_started,retire,failed);guard.submitted();}
  require(retire_count==1,"normal or resize-upload slot ownership added a queue wait");
  {FrameSubmissionGuard guard(external_started,[] {return false;},failed);}
  require(failed_count==1,"GPU retirement failure allowed unsafe query destruction");
  std::puts("frame_timing_probe passed=1 scope=owner_query_accounting gpu_execution=unqualified");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"frame_timing_probe failed=%s\n",error.what());return 1;}
