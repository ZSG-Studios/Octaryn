#include "GpuCounterState.h"
#include "FrameWatchdog.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <cmath>

namespace octaryn::client::rendering {
#ifdef _WIN32
namespace {
using State=GpuCounterProfile::State;
void SLANG_MCALL retain(void* object) {static_cast<State*>(object)->retain();}
void SLANG_MCALL release(void* object) {static_cast<State*>(object)->release();}
void SLANG_MCALL record(const rhi::ExecuteCallbackContext* context,void* object,const void* data,rhi::Size size) {
  auto& state=*static_cast<State*>(object);
  if(state.closed || size!=sizeof(bool))return;
  const bool begin=*static_cast<const bool*>(data);
  if(begin) {
    if(state.phase!=CounterPhase::Armed)return;
    if(context->nativeHandle.type!=rhi::NativeHandleType::D3D12GraphicsCommandList) {state.fail("native_command_list_unavailable");return;}
    state.phase=CounterPhase::Recording;
    state.session_open=state.check(state.api.GpaBeginSession(state.session),"begin_session");
    if(!state.session_open)return;
    state.list_open=state.check(state.api.GpaBeginCommandList(state.session,0,
        reinterpret_cast<void*>(context->nativeHandle.value),kGpaCommandListPrimary,&state.command_list),"begin_command_list");
    if(!state.list_open)return;
    state.sample_open=state.check(state.api.GpaBeginSample(0,state.command_list),"begin_sample");
  } else {
    // Even a partially failed begin is balanced while the native list is recording.
    if(state.sample_open && state.check(state.api.GpaEndSample(state.command_list),"end_sample"))state.sample_open=false;
    if(state.list_open && !state.sample_open && state.check(state.api.GpaEndCommandList(state.command_list),"end_command_list"))state.list_open=false;
    if(state.session_open && !state.list_open && state.check(state.api.GpaEndSession(state.session),"end_session"))state.session_open=false;
    if(state.phase==CounterPhase::Recording)state.phase=CounterPhase::Encoded;
  }
}
void callback(rhi::ICommandEncoder* commands,State* state,bool begin) {
  rhi::ExecuteCallbackDesc desc{};desc.callback=record;desc.userObject=state;
  desc.retainUserObject=retain;desc.releaseUserObject=release;
  desc.userData=&begin;desc.userDataSize=sizeof(begin);commands->executeCallback(desc);
}
}
std::unique_ptr<GpuCounterProfile> GpuCounterProfile::create(bool dx12) {
  const char* path=std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_PATH");
  if(!path || !*path)return {};
  auto result=std::unique_ptr<GpuCounterProfile>(new GpuCounterProfile(new State));
  result->state_->initialize(dx12);return result;
}
GpuCounterProfile::~GpuCounterProfile() {shutdown();state_->release();}
void GpuCounterProfile::attach(rhi::IDevice* device) {state_->attach(device);}
void GpuCounterProfile::begin(rhi::ICommandEncoder* commands,std::uint64_t frame,bool eligible,std::uint64_t revision) {
  auto& state=*state_;
  if(state.phase!=CounterPhase::Armed || state.queued)return;
  if(!eligible || !state.was_ready || state.scene_revision!=revision)state.ready_since=frame;
  state.was_ready=eligible;state.scene_revision=revision;
  if(frame<state.target)return;
  if(frame-state.target>120) {state.fail("eligible_reflection_frame_not_reached");return;}
  if(!eligible || frame-state.ready_since<64)return;
  state.frame=frame;state.queued=true;callback(commands,&state,true);
}
void GpuCounterProfile::end(rhi::ICommandEncoder* commands,const GpuCounterObservation& observation) {
  if(state_->queued) {state_->observation=observation;callback(commands,state_,false);state_->queued=false;}
}
void GpuCounterProfile::submitted(rhi::IFence* fence,std::uint64_t signal) {
  auto& state=*state_;
  if(state.phase!=CounterPhase::Encoded)return;
  state.fence=fence;state.signal=signal;state.phase=CounterPhase::Submitted;
  state.submitted_at=std::chrono::steady_clock::now();
  state.report<<"{\"event\":\"submitted\",\"renderer_frame\":"<<state.frame
      <<",\"frame_fence\":"<<signal<<",\"sample_id\":0,\"pass_index\":0,\"scene_revision\":"<<state.scene_revision
      <<",\"ready_frames\":"<<(state.frame-state.ready_since)<<",\"requested_and_ray_scene_ready\":true}\n";
  const auto& o=state.observation;
  state.report<<std::setprecision(9)<<"{\"event\":\"observation\",\"renderer_frame\":"<<state.frame
      <<",\"ready_frame\":"<<o.ready_frame<<",\"sampling_frame\":"<<o.sampling_frame
      <<",\"reflection_sampling_frame\":"<<o.reflection_sampling_frame<<",\"delta_ms\":"<<o.delta_ms
      <<",\"fixed_sampling\":"<<(o.fixed_sampling?"true":"false")<<",\"dimensions\":[";
  for(size_t i=0;i<o.dimensions.size();++i)state.report<<(i?",":"")<<o.dimensions[i];
  state.report<<"],\"camera\":[";
  for(size_t i=0;i<o.camera.size();++i)state.report<<(i?",":"")<<o.camera[i];
  state.report<<"]}\n";
  state.flush_report();
}
void GpuCounterProfile::poll() {
  auto& state=*state_;
  if(state.closed || state.phase!=CounterPhase::Submitted)return;
  if(++state.polls>600 || std::chrono::steady_clock::now()-state.submitted_at>std::chrono::seconds(10)) {
    state.fail("nonblocking_readback_deadline");return;
  }
  std::uint64_t completed{};
  if(SLANG_FAILED(state.fence->getCurrentValue(&completed)) || completed==UINT64_MAX) {state.fail("frame_fence_failed");return;}
  if(completed<state.signal)return;
  const auto ready=state.api.GpaIsSessionComplete(state.session);
  if(ready==kGpaStatusResultNotReady)return;
  if(!state.check(ready,"session_complete"))return;
  size_t bytes{};
  if(!state.check(state.api.GpaGetSampleResultSize(state.session,0,&bytes),"result_size"))return;
  if(bytes!=state.enabled.size()*sizeof(std::uint64_t) || bytes>64) {state.fail("result_size_mismatch");return;}
  std::uint64_t results[8]{};
  if(!state.check(state.api.GpaGetSampleResult(state.session,0,bytes,results),"sample_result"))return;
  for(size_t index=0;index<state.enabled.size();++index) {
    const auto& counter=state.enabled[index];
    state.report<<"{\"event\":\"result\",\"index\":"<<counter.index<<",\"name\":"
        <<counter_json_string(counter.name.c_str())<<",\"type\":"<<int(counter.type)<<",\"value\":";
    if(counter.type==kGpaDataTypeUint64)state.report<<results[index];
    else {double value{};std::memcpy(&value,&results[index],sizeof(value));
      if(!std::isfinite(value)) {state.report<<"null}\n";state.fail("nonfinite_counter_result");return;}
      state.report<<std::setprecision(17)<<value;}
    state.report<<"}\n";
  }
  state.report<<"{\"event\":\"complete\",\"renderer_frame\":"<<state.frame<<",\"polls\":"<<state.polls
      <<",\"timing_qualification\":false,\"occupancy_claim\":false}\n";
  if(!state.flush_report())return;
  state.phase=CounterPhase::Complete;
  std::printf("gpu_counter_profile status=complete frame=%llu counters=%zu passes=1\n",
      static_cast<unsigned long long>(state.frame),state.enabled.size());
}
void GpuCounterProfile::shutdown() {
  if(!state_ || state_->closed)return;
  poll();
  if(state_->sample_open || state_->list_open || state_->session_open) {
    state_->fail("recording_scope_unbalanced");frame_gpu_shutdown_failed("gpu_counter_scope");
  }
  state_->close();
}
#else
std::unique_ptr<GpuCounterProfile> GpuCounterProfile::create(bool) {
  if(const auto* path=std::getenv("OCTARYN_CLIENT_GPU_COUNTERS_PATH");path && *path)
    std::fputs("gpu_counter_profile status=unsupported requires=windows_dx12\n",stderr);
  return {};
}
GpuCounterProfile::~GpuCounterProfile()=default;
void GpuCounterProfile::attach(rhi::IDevice*) {}
void GpuCounterProfile::begin(rhi::ICommandEncoder*,std::uint64_t,bool,std::uint64_t) {}
void GpuCounterProfile::end(rhi::ICommandEncoder*,const GpuCounterObservation&) {}
void GpuCounterProfile::submitted(rhi::IFence*,std::uint64_t) {}
void GpuCounterProfile::poll() {}
void GpuCounterProfile::shutdown() {}
#endif
}
