#include "WorldRayAllocator.h"
#include "FrameWatchdog.h"
#include "WorldResourceBudget.h"
#include "../../Threading/BackgroundThread.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <thread>
#include <stdexcept>
namespace octaryn::client::rendering {
namespace {
template<class Predicate> bool wait_for(std::condition_variable& changed,std::unique_lock<std::mutex>& lock,
    std::uint64_t timeout,Predicate predicate) {
  if(timeout==UINT64_MAX) {changed.wait(lock,predicate);return true;}
  return changed.wait_for(lock,std::chrono::nanoseconds(std::min(timeout,std::uint64_t(INT64_MAX))),predicate);
}
bool grow(rhi::IDevice* device,WorldRayAllocation& work,std::uint64_t bytes,unsigned stride,
    rhi::BufferUsage usage,rhi::ResourceState initial,Slang::ComPtr<rhi::IBuffer>& buffer,const char* step) {
  if(buffer && buffer->getDesc().size>=std::max<std::uint64_t>(bytes,stride))return true;
  if(work.cancelled.load(std::memory_order_relaxed)) {work.result=SLANG_E_NOT_AVAILABLE;return false;}
  work.step=step;
  const auto elements=(std::max<std::uint64_t>(bytes,stride)+stride-1)/stride;
  rhi::BufferDesc desc{};desc.size=std::bit_ceil(elements)*stride;desc.elementSize=stride;
  desc.usage=usage;desc.defaultState=initial;
  work.result=device->createBuffer(desc,nullptr,buffer.writeRef());
  if(SLANG_FAILED(work.result))return false;
  work.buffer_bytes.fetch_add(desc.size,std::memory_order_relaxed);++work.buffers_created;return true;
}
}
bool WorldRayAllocation::wait(std::uint64_t timeout_ns) {
  std::unique_lock lock(mutex);
  return wait_for(changed,lock,timeout_ns,[&]{return complete.load(std::memory_order_acquire);});
}
struct WorldRayAllocator::State {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> graphics_queue;
  CreateAcceleration create{};void* context{};
  mutable std::mutex mutex;
  std::condition_variable changed;
  std::array<std::shared_ptr<WorldRayAllocation>,Capacity> queue,retained;
  std::shared_ptr<WorldRayAllocation> active;
  std::size_t first{},count{};
  std::atomic<std::uint64_t> allocations{};
  std::uint64_t buffers_created{};
  std::atomic<double> milliseconds{};
  bool stopping{},summary_logged{},resource_paused{};
  std::uint64_t admission_pauses{};
  std::atomic<bool> exited{};
  std::thread worker;
  State(rhi::IDevice* value,CreateAcceleration callback,void* user):
      device(value),create(callback),context(user) {
    if(!device || SLANG_FAILED(device->getQueue(rhi::QueueType::Graphics,graphics_queue.writeRef())))
      throw std::runtime_error("Ray allocation graphics queue unavailable");
    worker=std::thread([this]{run();});
  }
  void allocate(WorldRayAllocation& work) {
    work.result=SLANG_E_NOT_AVAILABLE;
    if(work.cancelled.load(std::memory_order_relaxed))return;
    const auto usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::AccelerationStructureBuildInput;
    if(!grow(device,work,std::uint64_t(work.faces)*24,24,usage,
        rhi::ResourceState::AccelerationStructureBuildInput,work.bounds,"bounds_buffer"))return;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=work.bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=24;input.proceduralPrimitives.primitiveCount=work.faces;
    input.proceduralPrimitives.flags=rhi::AccelerationStructureGeometryFlags::None;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    rhi::AccelerationStructureSizes sizes{};work.step="blas_sizes";
    work.result=device->getAccelerationStructureSizes(build,&sizes);
    if(SLANG_FAILED(work.result))return;
    if(!sizes.accelerationStructureSize) {work.result=SLANG_FAIL;return;}
    if(work.cancelled.load(std::memory_order_relaxed)) {work.result=SLANG_E_NOT_AVAILABLE;return;}
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::BottomLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_column";work.step="blas_create";
    work.result=create?create(context,desc,work.blas.writeRef()):device->createAccelerationStructure(desc,work.blas.writeRef());
    if(SLANG_FAILED(work.result))return;
    work.blas_bytes.store(desc.size,std::memory_order_relaxed);++allocations;
    if(!grow(device,work,sizes.scratchSize,4,rhi::BufferUsage::UnorderedAccess,
        rhi::ResourceState::UnorderedAccess,work.scratch,"blas_scratch"))return;
    work.step="complete";work.result=SLANG_OK;
  }
  void run() {
    threading::set_background_thread_priority("ray_allocation");
    for(;;) {
      std::shared_ptr<WorldRayAllocation> work;
      {
        std::unique_lock lock(mutex);changed.wait(lock,[&]{return stopping || count;});
        if(!count)break;
        work=std::move(queue[first]);first=(first+1)%Capacity;--count;active=work;
      }
      const auto start=std::chrono::steady_clock::now();
      try {allocate(*work);}catch(...) {work->result=SLANG_FAIL;}
      work->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      milliseconds.fetch_add(work->milliseconds,std::memory_order_relaxed);buffers_created+=work->buffers_created;
      {std::lock_guard lock(work->mutex);work->complete.store(true,std::memory_order_release);}
      work->changed.notify_all();
      {std::lock_guard lock(mutex);active.reset();}
    }
    {std::lock_guard lock(mutex);exited.store(true,std::memory_order_release);}
    changed.notify_all();
  }
};
WorldRayAllocator::WorldRayAllocator(rhi::IDevice* device,CreateAcceleration create,void* context):
    state_(std::make_unique<State>(device,create,context)) {}
WorldRayAllocator::~WorldRayAllocator() {
  stop();if(!finish(frame_fence_timeout_ms()*1000000ull))frame_gpu_shutdown_failed("ray_allocator");
}
std::shared_ptr<WorldRayAllocation> WorldRayAllocator::request(std::uint32_t faces,rhi::IBuffer* bounds,rhi::IBuffer* scratch) {
  auto& s=*state_;const bool allowed=world_resource_budget_allows(s.graphics_queue,s.resource_paused);
  collect();if(!allowed) {++s.admission_pauses;return {};}
  std::lock_guard lock(s.mutex);
  const auto slot=std::find(s.retained.begin(),s.retained.end(),nullptr);
  if(!faces || s.stopping || slot==s.retained.end())return {};
  auto work=std::make_shared<WorldRayAllocation>();work->faces=faces;work->bounds=bounds;work->scratch=scratch;
  *slot=work;s.queue[(s.first+s.count++)%Capacity]=work;s.changed.notify_one();return work;
}
void WorldRayAllocator::collect() {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  for(auto& work:s.retained)if(work && work.use_count()==1 && work->complete.load(std::memory_order_acquire))work.reset();
}
void WorldRayAllocator::stop() {
  auto& s=*state_;std::lock_guard lock(s.mutex);s.stopping=true;
  if(s.active)s.active->cancelled.store(true,std::memory_order_relaxed);
  for(auto& work:s.queue)if(work)work->cancelled.store(true,std::memory_order_relaxed);
  s.changed.notify_one();
}
bool WorldRayAllocator::finished() const {return state_->exited.load(std::memory_order_acquire);}
std::uint64_t WorldRayAllocator::created() const {return state_->allocations.load(std::memory_order_relaxed);}
double WorldRayAllocator::milliseconds() const {return state_->milliseconds.load(std::memory_order_relaxed);}
std::size_t WorldRayAllocator::pending() const {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  return std::count_if(s.retained.begin(),s.retained.end(),[](const auto& work){return bool(work);});
}
std::uint64_t WorldRayAllocator::bytes() const {
  auto& s=*state_;std::lock_guard lock(s.mutex);std::uint64_t bytes{};
  for(const auto& work:s.retained)if(work)bytes+=work->blas_bytes.load(std::memory_order_relaxed)+work->buffer_bytes.load(std::memory_order_relaxed);
  return bytes;
}
bool WorldRayAllocator::finish(std::uint64_t timeout_ns) {
  auto& s=*state_;std::unique_lock lock(s.mutex);
  if(!wait_for(s.changed,lock,timeout_ns,[&]{return s.exited.load(std::memory_order_acquire);}))return false;
  lock.unlock();if(s.worker.joinable())s.worker.join();collect();
  if(!s.summary_logged) {
    std::printf("world_ray_allocator blas_created=%llu buffers_created=%llu worker_ms=%.6f admission_pauses=%llu\n",
      static_cast<unsigned long long>(created()),static_cast<unsigned long long>(s.buffers_created),milliseconds(),
      static_cast<unsigned long long>(s.admission_pauses));
    std::fflush(stdout);s.summary_logged=true;
  }
  return true;
}
}
