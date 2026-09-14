#include "WorldRendererInternal.h"
#include "WorldMeshJob.h"
#include <cstring>
#include <algorithm>
namespace octaryn::client::rendering {
struct WorldMeshJob::State {
  WorldMeshTimings* timings{};
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IFence> fence;
  Slang::ComPtr<rhi::IBuffer> blocks,info,readback;
  Slang::ComPtr<rhi::ICommandBuffer> submission;
  WorldColumnGpu gpu,count;
  WorldMeshJobResources resources;
  std::uint64_t signal{},observed{};
  std::uint32_t voxels{};
  bool emitting{},finished{};
  bool buffer(std::uint64_t size,std::uint32_t stride,rhi::BufferUsage usage,
              rhi::ResourceState initial,Slang::ComPtr<rhi::IBuffer>& out,
              rhi::MemoryType memory=rhi::MemoryType::DeviceLocal,bool reuse=false) {
    if(reuse && out && out->getDesc().size>=size)return true;
    WorldMeshTimer timer(timings?&timings->allocation:nullptr);
    rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;desc.usage=usage;
    desc.defaultState=initial;desc.memoryType=memory;
    if(!world_rhi_ok(device->createBuffer(desc,nullptr,out.writeRef())))return false;
    ++resources.buffers_created;return true;
  }
  bool dispatch(WorldRenderer& r,rhi::ICommandEncoder* commands,std::uint32_t capacity) {
    auto& target=emitting?gpu:count;
    std::array<std::uint32_t,40> arguments{};std::uint32_t base{},patch_base{};
    for(std::size_t p=0;p<5;++p) {
      arguments[p*4]=6;arguments[p*4+3]=base;base+=target.pass_counts[p];
      arguments[20+p*4]=6;arguments[20+p*4+3]=patch_base;patch_base+=target.patch_counts[p];
    }
    const auto faces=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource;
    const auto args=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::IndirectArgument|
        rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
    if(!buffer(std::max(1u,capacity)*16ull,16,faces,rhi::ResourceState::ShaderResource,target.faces,rhi::MemoryType::DeviceLocal,!emitting) ||
       !buffer(std::max(1u,target.pass_counts[3]+target.pass_counts[4])*32ull,32,faces,rhi::ResourceState::ShaderResource,target.fluids,rhi::MemoryType::DeviceLocal,!emitting) ||
       !buffer(std::max(1u,patch_base)*4ull,4,faces,rhi::ResourceState::ShaderResource,target.patches,rhi::MemoryType::DeviceLocal,!emitting) ||
       !buffer(sizeof(arguments),4,args,rhi::ResourceState::IndirectArgument,target.arguments,rhi::MemoryType::DeviceLocal,!emitting))return false;
    {
      WorldMeshTimer timer(timings?&timings->upload:nullptr);
      if(!world_rhi_ok(commands->uploadBufferData(target.arguments,0,sizeof(arguments),arguments.data())))return false;
    }
    {
    WorldMeshTimer timer(timings?&timings->encoding:nullptr);
    auto* compute=commands->beginComputePass();if(!compute)return false;
    auto* root=compute->bindPipeline(r.mesh_pipeline);const std::uint32_t mode=emitting?1u:0u;
    const std::array<rhi::IBuffer*,7> buffers{blocks,info,target.faces,target.arguments,world_atlas_materials(r.atlas),target.fluids,target.patches};
    bool success=root && world_rhi_ok(root->setData({0,0,0},&mode,sizeof(mode)));
    for(std::uint32_t i=0;success && i<buffers.size();++i)
      success=world_rhi_ok(root->setBinding({0,i,0},rhi::Binding(buffers[i])));
    const auto groups=6u*32u*((voxels/(32u*32u)+31u)/32u);
    if(success)compute->dispatchCompute(groups+(voxels+31u)/32u,1,1);
    compute->end();if(!success)return false;
    if(!emitting)commands->copyBuffer(readback,0,target.arguments,0,160);
    submission=commands->finish();if(!submission)return false;
    }
    rhi::ICommandBuffer* command=submission; rhi::IFence* completed=fence;
    const auto value=signal+1;
    rhi::SubmitDesc submit{};submit.commandBuffers=&command;submit.commandBufferCount=1;
    submit.signalFences=&completed;submit.signalFenceValues=&value;submit.signalFenceCount=1;
    {
      WorldMeshTimer timer(timings?&timings->submission:nullptr);
      if(!world_rhi_ok(r.queue->submit(submit)))return false;
    }
    if(timings) {if(emitting)++timings->emit_submits;else ++timings->count_submits;}
    signal=value;return true;
  }
};
WorldMeshJob::WorldMeshJob():state_(std::make_unique<State>()) {}
WorldMeshJob::~WorldMeshJob() {
  wait();
  WorldMeshTimer timer(state_->timings?&state_->timings->release:nullptr);
  state_.reset();
}
bool WorldMeshJob::wait(std::uint64_t timeout) {
  auto& s=*state_;if(!s.signal || s.finished)return true;
  WorldMeshTimer timer(s.timings?&s.timings->fence_wait:nullptr);
  rhi::IFence* fence=s.fence;
  return world_rhi_ok(s.device->waitForFences(1,&fence,&s.signal,true,timeout));
}
bool WorldMeshJob::start(WorldRenderer& r,const world_presentation::StreamColumn& source) {
  auto& s=*state_;
  if((s.device && s.device.get()!=r.device.get()) || (s.signal && !s.finished) || source.height<=0 || source.height>512 || source.blocks.size()!=std::size_t(source.height)*1024) {
    r.status="invalid_column_payload";return false;
  }
  s.timings=r.gpu_profile?&r.mesh_timings:nullptr;
  if(s.timings)++s.timings->jobs_started;
  s.device=r.device;s.voxels=static_cast<std::uint32_t>(source.blocks.size());
  // A completed job retains only reusable scratch. Every emitted mesh remains
  // independently owned by its published column, never rewritten by reuse.
  s.emitting=false;s.finished=false;
  {
    WorldMeshTimer timer(s.timings?&s.timings->release:nullptr);
    s.gpu={};s.submission.setNull();
  }
  s.count.pass_counts={};s.count.patch_counts={};
  rhi::FenceDesc fence{};fence.label="halo_mesh_completion";
  if(!s.fence) {
    WorldMeshTimer timer(s.timings?&s.timings->allocation:nullptr);
    if(!world_rhi_ok(r.device->createFence(fence,s.fence.writeRef())))return false;
    ++s.resources.fences_created;
  }
  std::vector<std::uint32_t> halo;
  {
    WorldMeshTimer timer(s.timings?&s.timings->halo_decode:nullptr);
    halo=world_mesh_halo(r,source);
  }
  const std::array<std::int32_t,4> info{source.x,source.z,source.min_y,source.height};
  const auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
  if(!s.buffer(halo.size()*4,4,usage,rhi::ResourceState::ShaderResource,s.blocks,rhi::MemoryType::DeviceLocal,true) ||
      !s.buffer(sizeof(info),sizeof(info),usage,rhi::ResourceState::ShaderResource,s.info,rhi::MemoryType::DeviceLocal,true) ||
      !s.buffer(160,4,rhi::BufferUsage::CopyDestination,rhi::ResourceState::CopyDestination,s.readback,rhi::MemoryType::ReadBack,true))return false;
  Slang::ComPtr<rhi::ICommandEncoder> commands;
  {
    WorldMeshTimer timer(s.timings?&s.timings->encoding:nullptr);
    commands=r.queue->createCommandEncoder();if(!commands)return false;
  }
  {
    WorldMeshTimer timer(s.timings?&s.timings->upload:nullptr);
    if(!world_rhi_ok(commands->uploadBufferData(s.blocks,0,halo.size()*4,halo.data())) ||
        !world_rhi_ok(commands->uploadBufferData(s.info,0,sizeof(info),info.data())))return false;
  }
  s.gpu.min_y=source.min_y;s.gpu.height=source.height;
  const bool submitted=s.dispatch(r,commands,1);
  {
    WorldMeshTimer timer(s.timings?&s.timings->release:nullptr);
    commands.setNull();std::vector<std::uint32_t>().swap(halo);
  }
  return submitted;
}
bool WorldMeshJob::poll(WorldRenderer& r,WorldColumnGpu& output,bool& complete) {
  auto& s=*state_;complete=false;
  if(!s.signal || s.finished)return false;
  {
    WorldMeshTimer timer(s.timings?&s.timings->readback:nullptr);
    // A zero-timeout host wait verifies completion without blocking the frame.
    // Only a successful wait permits CPU readback or publication of this phase.
    rhi::IFence* fence=s.fence;
    const auto result=s.device->waitForFences(1,&fence,&s.signal,true,0);
    ++s.resources.poll_calls;s.resources.last_poll_result=result;
    if(result==SLANG_E_TIME_OUT) {++s.resources.poll_timeouts;return true;}
    if(!world_rhi_ok(result))return false;
  }
  s.observed=s.signal;
  if(s.emitting) {output=std::move(s.gpu);s.finished=true;complete=true;return true;}
  std::array<std::uint32_t,40> counts{};void* mapped{};
  {
  WorldMeshTimer timer(s.timings?&s.timings->readback:nullptr);
  if(!world_rhi_ok(s.device->mapBuffer(s.readback,rhi::CpuAccessMode::Read,&mapped)) || !mapped)return false;
  std::memcpy(counts.data(),mapped,sizeof(counts));
  if(!world_rhi_ok(s.device->unmapBuffer(s.readback)))return false;
  }
  std::uint32_t faces{},patches{};
  for(std::size_t p=0;p<5;++p) {
    s.gpu.pass_counts[p]=counts[p*4+1];faces+=s.gpu.pass_counts[p];
    s.gpu.patch_counts[p]=counts[20+p*4+1];patches+=s.gpu.patch_counts[p];
  }
  if(faces>std::uint64_t(s.voxels)*6 || patches<faces || patches>std::uint64_t(s.voxels)*6) {
    r.status="invalid_gpu_face_count";return false;
  }
  s.gpu.face_count=faces;s.emitting=true;
  Slang::ComPtr<rhi::ICommandEncoder> commands;
  {
    WorldMeshTimer timer(s.timings?&s.timings->encoding:nullptr);
    commands=r.queue->createCommandEncoder();
  }
  const bool submitted=commands && s.dispatch(r,commands,faces);
  {
    WorldMeshTimer timer(s.timings?&s.timings->release:nullptr);
    commands.setNull();
  }
  return submitted;
}
std::uint64_t WorldMeshJob::gpu_bytes() const {
  const auto& s=*state_;std::uint64_t bytes=resources().scratch_bytes;
  for(auto* buffer:{s.gpu.faces.get(),s.gpu.arguments.get(),s.gpu.fluids.get(),s.gpu.patches.get()})
    if(buffer)bytes+=buffer->getDesc().size;
  return bytes;
}
WorldMeshJobResources WorldMeshJob::resources() const {
  const auto& s=*state_;auto result=s.resources;
  result.input_capacity=s.blocks?s.blocks->getDesc().size:0;
  result.signal_value=s.signal;result.observed_value=s.observed;
  result.emitting=s.emitting;result.finished=s.finished;
  for(auto* buffer:{s.blocks.get(),s.info.get(),s.readback.get(),s.count.faces.get(),s.count.arguments.get(),s.count.fluids.get(),s.count.patches.get()})
    if(buffer)result.scratch_bytes+=buffer->getDesc().size;
  return result;
}
}
