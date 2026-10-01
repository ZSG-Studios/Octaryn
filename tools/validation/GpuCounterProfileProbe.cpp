// Real diagnostic owner, fake GPA dispatch and fence. No graphics device or DLL.
#include <slang-rhi.h>
#include <cstdint>
#include <memory>
#define private public
#include "GpuCounterProfile.h"
#undef private
#include "GpuCounterProfile.cpp"
#include <cassert>
#include <filesystem>
#include <vector>
#include <algorithm>

using namespace octaryn::client::rendering;
namespace {
using State=GpuCounterProfile::State;
struct Fence final:rhi::IFence {
  std::uint64_t value{};unsigned refs{1};
  SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID&,void**) override {return SLANG_E_NO_INTERFACE;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL addRef() override {return ++refs;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL release() override {const auto left=--refs;if(!left)delete this;return left;}
  SLANG_NO_THROW SlangResult SLANG_MCALL getCurrentValue(std::uint64_t* output) override {*output=value;return SLANG_OK;}
  SLANG_NO_THROW SlangResult SLANG_MCALL setCurrentValue(std::uint64_t input) override {value=input;return SLANG_OK;}
  SLANG_NO_THROW SlangResult SLANG_MCALL getNativeHandle(rhi::NativeHandle*) override {return SLANG_E_NOT_AVAILABLE;}
  SLANG_NO_THROW SlangResult SLANG_MCALL getSharedHandle(rhi::NativeHandle*) override {return SLANG_E_NOT_AVAILABLE;}
};
unsigned result_reads{},end_lists{},end_sessions{},deletes{},samples{};
GpaStatus ready=kGpaStatusResultNotReady,begin_result=kGpaStatusOk,end_result=kGpaStatusOk;
std::vector<GpaUInt32> selected;
const char* names[]={"GPUTime","CSBusy","L2CacheHit","WaveOccupancyPct"};
State* state(const std::filesystem::path& path) {
  auto* s=new State;s->report.open(path,std::ios::binary);assert(s->report);
  s->phase=CounterPhase::Armed;s->session=reinterpret_cast<GpaSessionId>(1);
  s->enabled={{0,"GPUTime",kGpaDataTypeUint64},{1,"CSBusy",kGpaDataTypeFloat64}};
  s->api.GpaGetStatusAsStr=[](GpaStatus){return "fake status";};
  s->api.GpaBeginSession=[](GpaSessionId){return kGpaStatusOk;};
  s->api.GpaBeginCommandList=[](GpaSessionId,GpaUInt32 pass,void*,GpaCommandListType type,GpaCommandListId* list){
    assert(pass==0 && type==kGpaCommandListPrimary);*list=reinterpret_cast<GpaCommandListId>(2);return kGpaStatusOk;};
  s->api.GpaBeginSample=[](GpaUInt32 id,GpaCommandListId){assert(id==0);++samples;return begin_result;};
  s->api.GpaEndSample=[](GpaCommandListId){return end_result;};
  s->api.GpaEndCommandList=[](GpaCommandListId){++end_lists;return kGpaStatusOk;};
  s->api.GpaEndSession=[](GpaSessionId){++end_sessions;return kGpaStatusOk;};
  s->api.GpaIsSessionComplete=[](GpaSessionId){return ready;};
  s->api.GpaGetSampleResultSize=[](GpaSessionId,GpaUInt32,size_t* size){*size=16;return kGpaStatusOk;};
  s->api.GpaGetSampleResult=[](GpaSessionId,GpaUInt32,size_t size,void* out){
    assert(size==16);++result_reads;std::uint64_t raw[2]={42,0};double busy=12.5;
    std::memcpy(&raw[1],&busy,8);std::memcpy(out,raw,16);return kGpaStatusOk;};
  s->api.GpaDeleteSession=[](GpaSessionId){++deletes;return kGpaStatusOk;};
  return s;
}
void encode(State& s) {
  rhi::ExecuteCallbackContext context{};
  context.nativeHandle={rhi::NativeHandleType::D3D12GraphicsCommandList,3};
  bool begin=true;record(&context,&s,&begin,sizeof(begin));
  begin=false;record(&context,&s,&begin,sizeof(begin));
}
void inventory(State& s) {
  selected.clear();s.enabled.clear();s.phase=CounterPhase::Setup;
  s.requested={"GPUTime","CSBusy","L2CacheHit","WaveOccupancyPct"};
  s.api.GpaGetNumCounters=[](GpaSessionId,GpaUInt32* count){*count=4;return kGpaStatusOk;};
  s.api.GpaGetCounterName=[](GpaSessionId,GpaUInt32 i,const char** text){*text=names[i];return kGpaStatusOk;};
  s.api.GpaGetCounterDescription=[](GpaSessionId,GpaUInt32,const char** text){*text="description\nquoted \"text\"";return kGpaStatusOk;};
  s.api.GpaGetCounterGroup=[](GpaSessionId,GpaUInt32,const char** text){*text="group";return kGpaStatusOk;};
  s.api.GpaGetCounterDataType=[](GpaSessionId,GpaUInt32,GpaDataType* type){*type=kGpaDataTypeUint64;return kGpaStatusOk;};
  s.api.GpaGetCounterUsageType=[](GpaSessionId,GpaUInt32,GpaUsageType* type){*type=kGpaUsageTypeItems;return kGpaStatusOk;};
  s.api.GpaGetCounterSampleType=[](GpaSessionId,GpaUInt32 i,GpaCounterSampleType* type){
    *type=i==3?kGpaCounterSampleTypeStreaming:kGpaCounterSampleTypeDiscrete;return kGpaStatusOk;};
  s.api.GpaGetCounterUuid=[](GpaSessionId,GpaUInt32 i,GpaUuid* uuid){*uuid={};uuid->Data1=i;return kGpaStatusOk;};
  s.api.GpaGetDataTypeAsStr=[](GpaDataType,const char** text){*text="uint64";return kGpaStatusOk;};
  s.api.GpaGetUsageTypeAsStr=[](GpaUsageType,const char** text){*text="items";return kGpaStatusOk;};
  s.api.GpaEnableCounter=[](GpaSessionId,GpaUInt32 i){selected.push_back(i);return kGpaStatusOk;};
  s.api.GpaDisableCounter=[](GpaSessionId,GpaUInt32 i){std::erase(selected,i);return kGpaStatusOk;};
  s.api.GpaGetPassCount=[](GpaSessionId,GpaUInt32* count){*count=selected.size()>2?2u:1u;return kGpaStatusOk;};
  s.api.GpaGetNumEnabledCounters=[](GpaSessionId,GpaUInt32* count){*count=static_cast<GpaUInt32>(selected.size());return kGpaStatusOk;};
  s.api.GpaGetEnabledIndex=[](GpaSessionId,GpaUInt32 ordinal,GpaUInt32* i){*i=selected[selected.size()-1-ordinal];return kGpaStatusOk;};
}
}
int main(int argc,char** argv) {
  assert(argc>=2);const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  _putenv_s("OCTARYN_CLIENT_GPU_COUNTERS_PATH","");
  assert(!GpuCounterProfile::create(true));
  if(argc==3) {
    auto* s=state(output/"unbalanced.jsonl");GpuCounterProfile p(s);
    end_result=kGpaStatusErrorFailed;encode(*s);p.shutdown();return 99;
  }
  {
    auto* s=state(output/"complete.jsonl");auto p=std::unique_ptr<GpuCounterProfile>(new GpuCounterProfile(s));
    retain(s);encode(*s);assert(s->phase==CounterPhase::Encoded);
    auto* fence=new Fence;p->submitted(fence,7);p->poll();assert(result_reads==0);
    fence->value=7;p->poll();assert(result_reads==0);
    ready=kGpaStatusOk;p->poll();assert(result_reads==1 && s->phase==CounterPhase::Complete);
    p->poll();assert(result_reads==1);p->shutdown();assert(deletes==1 && s->closed);
    p.reset();assert(s->references==1);release(s);fence->release();
  }
  {
    auto* s=state(output/"partial-begin.jsonl");GpuCounterProfile p(s);
    const auto before=end_sessions;begin_result=kGpaStatusErrorFailed;encode(*s);
    assert(s->phase==CounterPhase::Failed && !s->sample_open && !s->list_open && !s->session_open);
    assert(end_sessions==before+1);begin_result=kGpaStatusOk;
  }
  {
    auto* s=state(output/"timeout.jsonl");GpuCounterProfile p(s);encode(*s);
    auto* fence=new Fence;p.submitted(fence,100);s->submitted_at-=std::chrono::seconds(11);
    const auto before=deletes;p.poll();assert(s->phase==CounterPhase::Failed && deletes==before && s->session);
    fence->value=100;p.shutdown();assert(deletes==before+1);fence->release();
  }
  {
    auto* s=state(output/"inventory.jsonl");GpuCounterProfile p(s);inventory(*s);s->enumerate();
    assert(s->phase==CounterPhase::Armed && s->enabled.size()==2 && s->enabled[0].index==1);
  }
  {
    auto* s=state(output/"multipass.jsonl");GpuCounterProfile p(s);inventory(*s);
    s->requested.resize(3);s->explicit_names=true;s->enumerate();assert(s->phase==CounterPhase::Failed);
  }
  {
    auto* s=state(output/"not-ready.jsonl");GpuCounterProfile p(s);
    p.begin(nullptr,240,false,1);assert(samples==3 && !s->queued);
    p.begin(nullptr,361,false,1);assert(s->phase==CounterPhase::Failed);
  }
  {
    auto* s=state(output/"write-failure.jsonl");GpuCounterProfile p(s);encode(*s);
    auto* fence=new Fence;fence->value=7;p.submitted(fence,7);
    s->report.setstate(std::ios::badbit);p.poll();assert(s->phase==CounterPhase::Failed);fence->release();
  }
  std::puts("gpu_counter_native passed=disabled,single_pass,typed_results,fence,not_ready,timeout,partial_begin,retention,inventory,multipass,eligibility,write_failure");
}
