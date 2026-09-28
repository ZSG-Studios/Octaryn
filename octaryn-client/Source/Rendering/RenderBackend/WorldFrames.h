#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>
#include "FrameCpuTrace.h"
#include "FrameFenceWait.h"

namespace octaryn::client::rendering {
// Each slot owns mutable resources until its graphics submission completes.
class WorldFrames {
  Slang::ComPtr<rhi::IDevice> device_;
  Slang::ComPtr<rhi::IFence> fence_;
  std::array<std::uint64_t,2> pending_{};
  std::array<std::uint64_t,2> source_frames_{UINT64_MAX,UINT64_MAX};
  std::uint64_t submitted_{};
  unsigned count_{2};
public:
  WorldFrames()=default;
  WorldFrames(const WorldFrames&)=delete;
  WorldFrames& operator=(const WorldFrames&)=delete;
  ~WorldFrames() {drain();}
  bool initialize(rhi::IDevice* device,unsigned count) {
    if(count<1 || count>pending_.size() || fence_)return false;
    device_=device;count_=count;
    rhi::FenceDesc desc{};desc.label="world_frame_completion";
    return SLANG_SUCCEEDED(device_->createFence(desc,fence_.writeRef()));
  }
  unsigned count() const {return count_;}
  rhi::IFence* fence() const {return fence_.get();}
  std::uint64_t last_signal() const {return submitted_;}
  unsigned slot(std::uint64_t frame) const {return static_cast<unsigned>(frame%count_);}
  // timeout_ms bounds CPU blocking on a hung GPU; UINT64_MAX waits forever.
  // The RHI takes nanoseconds, so convert here; callers stay in milliseconds.
  bool wait(unsigned slot,std::uint64_t timeout_ms=UINT64_MAX,FrameCpuTrace* trace=nullptr) {
    if(slot>=count_)return false;
    rhi::IFence* fence=fence_;
    const std::uint64_t timeout_ns=timeout_ms==UINT64_MAX?UINT64_MAX:timeout_ms*1000000ull;
    const auto record=retire_frame_fence(slot,pending_[slot],source_frames_[slot],
        [&](std::uint64_t& completed) {return fence->getCurrentValue(&completed);},
        [&] {return device_->waitForFences(1,&fence,&pending_[slot],true,timeout_ns);},
        [&] {return trace?FrameCpuTrace::now():0;});
    if(trace)trace->fence(record);
    if(record.success)pending_[slot]=0;
    return record.success;
  }

  bool submit(rhi::ICommandQueue* queue,rhi::ICommandBuffer* command,unsigned slot,std::uint64_t source_frame=UINT64_MAX) {
    if(slot>=count_ || pending_[slot] || !fence_ || !command)return false;
    const auto signal=submitted_+1;rhi::IFence* fence=fence_;
    rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
    desc.signalFences=&fence;desc.signalFenceValues=&signal;desc.signalFenceCount=1;
    if(SLANG_FAILED(queue->submit(desc)))return false;
    submitted_=signal;pending_[slot]=signal;source_frames_[slot]=source_frame;return true;
  }
  // Signal after every queue submission, including meshing/capture work that
  // does not use a frame slot. All backends support fence-only submissions.
  bool synchronize(rhi::ICommandQueue* queue,std::uint64_t timeout_ms) {
    if(!fence_)return !queue;
    if(!queue)return false;
    const auto signal=submitted_+1;rhi::IFence* fence=fence_;
    rhi::SubmitDesc desc{};desc.signalFences=&fence;
    desc.signalFenceValues=&signal;desc.signalFenceCount=1;
    if(SLANG_FAILED(queue->submit(desc)))return false;
    submitted_=signal;pending_[0]=signal;source_frames_[0]=UINT64_MAX;
    if(!wait(0,timeout_ms))return false;
    pending_.fill(0);return true;
  }
  // Bounded so teardown cannot zombie the process on a hung GPU.
  bool drain(std::uint64_t timeout_ms=4000) {
    bool ok=true;
    for(unsigned slot=0;slot<count_;++slot)if(!wait(slot,timeout_ms))ok=false;
    return ok;
  }
};
}
