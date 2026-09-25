#include "WorldMeshAllocator.h"
#include "WorldResourceBudget.h"
#include "FrameWatchdog.h"
#include "../../Threading/BackgroundThread.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
namespace octaryn::client::rendering {
namespace {
template<class Predicate> bool wait_for(std::condition_variable& changed,std::unique_lock<std::mutex>& lock,
    std::uint64_t timeout,Predicate predicate) {
  if(timeout==UINT64_MAX) {changed.wait(lock,predicate);return true;}
  return changed.wait_for(lock,std::chrono::nanoseconds(std::min(timeout,std::uint64_t(INT64_MAX))),predicate);
}
}
bool WorldMeshAllocation::wait(std::uint64_t timeout_ns) {
  std::unique_lock lock(mutex);
  return wait_for(changed,lock,timeout_ns,[&]{return complete.load(std::memory_order_acquire);});
}
struct WorldMeshAllocator::State {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> graphics_queue;
  // Worker-only creation/access; final cache release follows the owner's join.
  Slang::ComPtr<rhi::IBuffer> empty_fluid;
  CreateBuffer create{};void* context{};
  std::mutex mutex;
  std::condition_variable changed;
  std::array<std::shared_ptr<WorldMeshAllocation>,Capacity> queue;
  std::array<std::shared_ptr<WorldMeshAllocation>,Capacity> retained;
  std::shared_ptr<WorldMeshAllocation> active;
  std::size_t first{},count{};
  std::uint64_t output_buffers_created{},empty_fluid_reuses{};
  std::uint64_t pressure_deferrals{},retirement_peak_count{},retirement_peak_bytes{};
  bool resource_paused{};
  bool stopping{},summary_logged{};
  std::atomic<bool> exited{};
  std::thread worker;
  State(rhi::IDevice* value,CreateBuffer callback,void* user):device(value),create(callback),context(user) {
    if(!device || SLANG_FAILED(device->getQueue(rhi::QueueType::Graphics,graphics_queue.writeRef())))
      throw std::runtime_error("Mesh allocator graphics queue unavailable");
    worker=std::thread([this]{run();});
  }
  void run() {
    threading::set_background_thread_priority("mesh_allocation");
    for(;;) {
      std::shared_ptr<WorldMeshAllocation> work;
      {
        std::unique_lock lock(mutex);changed.wait(lock,[&]{return stopping || count;});
        if(!count)break;
        work=std::move(queue[first]);first=(first+1)%Capacity;--count;active=work;
      }
      const auto start=std::chrono::steady_clock::now();work->result=SLANG_OK;
      try {
        for(std::size_t index=0;index<work->buffers.size();++index) {
          if(work->cancelled.load(std::memory_order_relaxed)) {work->result=SLANG_E_NOT_AVAILABLE;break;}
          auto& buffer=work->buffers[index];
          const auto& desc=work->descriptions[index];
          const bool share=index==1 && work->empty_fluids;
          if(share && (desc.size!=32 || desc.elementSize!=32 || desc.memoryType!=rhi::MemoryType::DeviceLocal ||
              desc.format!=rhi::Format::Undefined || desc.defaultState!=rhi::ResourceState::ShaderResource ||
              desc.usage!=(rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource))) {
            work->result=SLANG_E_INVALID_ARG;break;
          }
          if(share && empty_fluid) {buffer=empty_fluid;++work->empty_fluid_reuses;}
          else {
            work->result=create?create(context,desc,buffer.writeRef()):device->createBuffer(desc,nullptr,buffer.writeRef());
            if(SLANG_FAILED(work->result))break;
            ++work->created;
            if(share)empty_fluid=buffer;
          }
          work->bytes.fetch_add(buffer->getDesc().size,std::memory_order_relaxed);
        }
      } catch(...) {work->result=SLANG_FAIL;}
      output_buffers_created+=work->created;empty_fluid_reuses+=work->empty_fluid_reuses;
      work->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      {std::lock_guard lock(work->mutex);work->complete.store(true,std::memory_order_release);}
      work->changed.notify_all();
      {std::lock_guard lock(mutex);active.reset();}
    }
    {std::lock_guard lock(mutex);exited.store(true,std::memory_order_release);}
    changed.notify_all();
  }
};
WorldMeshAllocator::WorldMeshAllocator(rhi::IDevice* device,CreateBuffer create,void* context):
    state_(std::make_unique<State>(device,create,context)) {}
WorldMeshAllocator::~WorldMeshAllocator() {
  stop();
  if(!finish(frame_fence_timeout_ms()*1000000ull))frame_gpu_shutdown_failed("mesh_allocator");
}
std::shared_ptr<WorldMeshAllocation> WorldMeshAllocator::request(const std::array<rhi::BufferDesc,3>& descriptions,bool empty_fluids) {
  auto& s=*state_;rhi::ResourceRetirementInfo retirement{};
  const bool allowed=world_resource_budget_allows(s.graphics_queue,s.resource_paused,&retirement);
  s.retirement_peak_count=std::max(s.retirement_peak_count,retirement.peakCount);
  s.retirement_peak_bytes=std::max(s.retirement_peak_bytes,retirement.peakBufferBytes);
  if(!allowed)++s.pressure_deferrals;
  collect();
  if(!allowed)return {};
  std::lock_guard lock(s.mutex);
  const auto slot=std::find(s.retained.begin(),s.retained.end(),nullptr);
  if(s.stopping || slot==s.retained.end())return {};
  auto work=std::make_shared<WorldMeshAllocation>();work->descriptions=descriptions;work->empty_fluids=empty_fluids;
  *slot=work;
  s.queue[(s.first+s.count++)%Capacity]=work;s.changed.notify_one();return work;
}
void WorldMeshAllocator::collect() {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  for(auto& work:s.retained)if(work && work.use_count()==1 && work->complete.load(std::memory_order_acquire))work.reset();
}
void WorldMeshAllocator::stop() {
  auto& s=*state_;std::lock_guard lock(s.mutex);s.stopping=true;
  if(s.active)s.active->cancelled.store(true,std::memory_order_relaxed);
  for(auto& work:s.queue)if(work)work->cancelled.store(true,std::memory_order_relaxed);
  s.changed.notify_one();
}
bool WorldMeshAllocator::finished() const {return state_->exited.load(std::memory_order_acquire);}
std::size_t WorldMeshAllocator::pending() const {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  return std::count_if(s.retained.begin(),s.retained.end(),[](const auto& work){return bool(work);});
}
bool WorldMeshAllocator::finish(std::uint64_t timeout_ns) {
  auto& s=*state_;std::unique_lock lock(s.mutex);
  if(!wait_for(s.changed,lock,timeout_ns,[&]{return s.exited.load(std::memory_order_acquire);}))return false;
  lock.unlock();if(s.worker.joinable())s.worker.join();collect();
  if(!s.summary_logged) {
    rhi::ResourceRetirementInfo retirement{};
    const auto query=s.graphics_queue->getResourceRetirementInfo(&retirement);
    if(SLANG_FAILED(query)) {
      std::fprintf(stderr,"world_mesh_allocator retirement_query_failed result=%d\n",int(query));return false;
    }
    std::printf("world_mesh_allocator output_buffers_created=%llu empty_fluid_reuses=%llu retirement_deferrals=%llu "
        "retirement_peak_count=%llu retirement_peak_buffer_bytes=%llu retirement_pending_count=%llu "
        "retirement_pending_buffer_bytes=%llu retirement_active_count=%u retirement_async=%u\n",
        static_cast<unsigned long long>(s.output_buffers_created),static_cast<unsigned long long>(s.empty_fluid_reuses),
        static_cast<unsigned long long>(s.pressure_deferrals),
        static_cast<unsigned long long>(std::max(s.retirement_peak_count,retirement.peakCount)),
        static_cast<unsigned long long>(std::max(s.retirement_peak_bytes,retirement.peakBufferBytes)),
        static_cast<unsigned long long>(retirement.pendingCount),
        static_cast<unsigned long long>(retirement.pendingBufferBytes),retirement.activeCount,unsigned(retirement.asynchronous));
    std::fflush(stdout);s.summary_logged=true;
  }
  return true;
}
}
