#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>

namespace octaryn::client::rendering {
// Each slot owns mutable resources until its graphics submission completes.
class WorldFrames {
  Slang::ComPtr<rhi::IDevice> device_;
  Slang::ComPtr<rhi::IFence> fence_;
  std::array<std::uint64_t,2> pending_{};
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
  unsigned slot(std::uint64_t frame) const {return static_cast<unsigned>(frame%count_);}
  bool wait(unsigned slot) {
    if(slot>=count_)return false;
    if(!pending_[slot])return true;
    rhi::IFence* fence=fence_;
    if(SLANG_FAILED(device_->waitForFences(1,&fence,&pending_[slot],true,UINT64_MAX)))return false;
    pending_[slot]=0;return true;
  }
  bool submit(rhi::ICommandQueue* queue,rhi::ICommandBuffer* command,unsigned slot) {
    if(slot>=count_ || pending_[slot] || !fence_ || !command)return false;
    const auto signal=submitted_+1;rhi::IFence* fence=fence_;
    rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
    desc.signalFences=&fence;desc.signalFenceValues=&signal;desc.signalFenceCount=1;
    if(SLANG_FAILED(queue->submit(desc)))return false;
    submitted_=signal;pending_[slot]=signal;return true;
  }
  bool drain() {
    for(unsigned slot=0;slot<count_;++slot)if(!wait(slot))return false;
    return true;
  }
};
}
