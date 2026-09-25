#include "Probe.h"
#include "FrameWatchdog.h"
#include "RetirementDrain.h"
#include "WorldMeshJob.h"
#include "WorldResourceBudget.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace mesh_probe {
void resource_budget_cases() {
  constexpr std::uint64_t MiB=1024ull*1024;
  rhi::ResourceRetirementInfo info{};bool paused=true;
  info.pendingCount=1000;info.pendingBufferBytes=256*MiB;
  require(world_resource_budget_allows(info,paused) && !paused,"synchronous retirement was throttled");
  info.asynchronous=true;info.pendingCount=511;info.pendingBufferBytes=128*MiB-1;
  require(world_resource_budget_allows(info,paused),"retirement gate rejected below high water");
  info.pendingCount=512;
  require(!world_resource_budget_allows(info,paused),"retirement count high water admitted allocation");
  info.pendingCount=256;info.pendingBufferBytes=64*MiB+1;
  require(!world_resource_budget_allows(info,paused),"retirement resumed before byte low water");
  info.pendingBufferBytes=64*MiB;
  require(world_resource_budget_allows(info,paused),"retirement did not resume at both low waters");
  info.pendingCount=1;info.pendingBufferBytes=128*MiB;
  require(!world_resource_budget_allows(info,paused),"retirement byte high water admitted allocation");
  info.pendingCount=257;info.pendingBufferBytes=0;
  require(!world_resource_budget_allows(info,paused),"retirement resumed before count low water");
  info.pendingCount=256;info.peakCount=10000;info.peakBufferBytes=1024*MiB;
  require(world_resource_budget_allows(info,paused),"historical peak permanently paused allocation");
  std::puts("world_resource_budget=passed count_high=512 bytes_high_mib=128 count_low=256 bytes_low_mib=64 hysteresis=1 synchronous_bypass=1");
}
namespace {
struct Gate {
  rhi::IDevice* device;
  std::mutex mutex;std::condition_variable changed;
  bool entered{},released{},fail{};
  std::thread::id worker;
  explicit Gate(rhi::IDevice* value):device(value) {}
  void release() {std::lock_guard lock(mutex);released=true;changed.notify_all();}
  void wait() {
    std::unique_lock lock(mutex);
    require(changed.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}),"allocation worker did not enter callback");
  }
  static SlangResult create(void* context,const rhi::BufferDesc& desc,rhi::IBuffer** output) {
    auto& gate=*static_cast<Gate*>(context);std::unique_lock lock(gate.mutex);
    gate.worker=std::this_thread::get_id();gate.entered=true;gate.changed.notify_all();
    if(!gate.changed.wait_for(lock,std::chrono::seconds(5),[&]{return gate.released;}))return SLANG_FAIL;
    if(gate.fail)return SLANG_FAIL;
    lock.unlock();return gate.device->createBuffer(desc,nullptr,output);
  }
};
struct Cleanup {
  WorldRenderer& renderer;std::array<Gate*,3> gates;
  ~Cleanup() {
    for(auto* gate:gates)gate->release();
    if(renderer.mesh_allocator) {
      renderer.mesh_allocator->stop();
      if(!renderer.mesh_allocator->finish(2000000000ull))frame_gpu_shutdown_failed("mesh_probe_allocator_cleanup");
    }
  }
};
void allocator(WorldRenderer& r,Gate& gate) {
  if(r.mesh_allocator) {r.mesh_allocator->stop();require(r.mesh_allocator->finish(2000000000ull),"old allocation worker stop");}
  r.mesh_allocator=std::make_unique<WorldMeshAllocator>(r.device,Gate::create,&gate);
}
void count(WorldRenderer& r,WorldMeshJob& job,const StreamColumn& source,WorldColumnGpu& output) {
  require(job.start(r,source) && job.wait(1000000000ull),"allocation fixture count completion");
  bool complete{};
  require(job.poll(r,output,complete) && !complete && job.resources().allocating && !job.resources().emitting,
      "count completion did not enter private asynchronous allocation");
}
void emit(WorldRenderer& r,WorldMeshJob& job,WorldColumnGpu& output) {
  require(job.wait(1000000000ull),"shared-fluid output allocation completion");
  bool complete{};require(job.poll(r,output,complete) && !complete && job.resources().emitting,
      "shared-fluid buffers did not submit emit");
}
void finish(WorldRenderer& r,WorldMeshJob& job,WorldColumnGpu& output) {
  bool complete{};
  require(job.wait(1000000000ull) && job.poll(r,output,complete) && complete,"shared-fluid exact mesh completion");
  require(job.finished_counters()!=nullptr,"completed shared-fluid mesh counters unavailable");
}
}
void mesh_allocation_cases(Fixture& fixture) {
  resource_budget_cases();
  auto& r=fixture.renderer;const auto owner=std::this_thread::get_id();
  Gate ready(r.device),failure(r.device),cancel(r.device);Cleanup cleanup{r,{&ready,&failure,&cancel}};
  auto source=column();std::uint16_t stone{},water{};
  for(std::size_t i=0;i<fixture.catalog.size();++i) {
    if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=static_cast<std::uint16_t>(i);
    if(fixture.catalog[i].id=="octaryn.basegame.block.water")water=static_cast<std::uint16_t>(i);
  }
  require(stone!=0 && water!=0,"allocation fixture stone/water missing");put(source,3,4,5,stone);
  allocator(r,ready);WorldColumnGpu output;
  {
    WorldMeshJob job;count(r,job,source,output);ready.wait();
    require(ready.worker!=owner,"buffer allocation ran on the presentation owner");
    for(unsigned i=0;i<16;++i) {
      bool complete=true;require(job.poll(r,output,complete) && !complete && !output.faces,
          "pending allocation waited or exposed a mesh before emit");
    }
    require(r.frame_queue.synchronize(r.queue,1000),"main graphics queue stopped while allocation worker was blocked");
    require(r.columns.empty() && r.sources.empty() && job.resources().signal_value==1,
        "private allocation changed publication or submitted emit early");
    ready.release();require(job.wait(1000000000ull),"private output allocation completion");
    bool complete{};require(job.poll(r,output,complete) && !complete && job.resources().emitting,
        "ready buffers did not submit emit on the owner");
    require(job.wait(1000000000ull) && job.poll(r,output,complete) && complete,"allocated exact mesh completion");
    require(job.finished_counters()!=nullptr,"completed allocated mesh counters unavailable");
    require(job.resources().buffers_created==10 && job.resources().allocation_worker_ms>0,
        "worker allocation count/time was not retained");
    fixture.verify("async_allocated_exact_mesh",source,fixture.read_mesh(output,job.finished_counters()));
  }
  std::array<WorldColumnGpu,2> dry_outputs;
  {
    std::array<WorldMeshJob,2> jobs;
    for(unsigned i=0;i<jobs.size();++i)count(r,jobs[i],source,dry_outputs[i]);
    // Both emit submissions reference the same unused UAV before either is read.
    for(unsigned i=0;i<jobs.size();++i)emit(r,jobs[i],dry_outputs[i]);
    for(unsigned i=0;i<jobs.size();++i) {
      finish(r,jobs[i],dry_outputs[i]);
      require(dry_outputs[i].fluids.get()==output.fluids.get() && jobs[i].resources().buffers_created==9 &&
          jobs[i].resources().empty_fluid_reuses==1,"dry output did not reuse exactly one fluid allocation");
      fixture.verify("shared_empty_fluid_exact_mesh",source,fixture.read_mesh(dry_outputs[i],jobs[i].finished_counters()));
    }
  }
  auto wet=source;put(wet,3,4,5,water);
  for(const auto& p:std::array<std::array<int,3>,5>{{{2,4,5},{4,4,5},{3,3,5},{3,4,4},{3,4,6}}})
    put(wet,p[0],p[1],p[2],stone);
  WorldColumnGpu wet_output;
  {
    WorldMeshJob job;count(r,job,wet,wet_output);emit(r,job,wet_output);finish(r,job,wet_output);
    require(wet_output.pass_counts[3]+wet_output.pass_counts[4]==1 && wet_output.fluids->getDesc().size==32 &&
        wet_output.fluids.get()!=output.fluids.get() && job.resources().buffers_created==10 &&
        job.resources().empty_fluid_reuses==0,"one real fluid face incorrectly reused the empty 32-byte buffer");
    const auto mesh=fixture.read_mesh(wet_output,job.finished_counters());
    fixture.verify("single_fluid_face_unique_buffer",wet,mesh);fixture.verify_fluids(wet,mesh);
  }
  allocator(r,failure);failure.fail=true;
  {
    WorldMeshJob job;WorldColumnGpu failed;count(r,job,source,failed);failure.wait();failure.release();
    require(job.wait(1000000000ull),"failed allocation completion notification");
    bool complete{};require(!job.poll(r,failed,complete) && !complete && !failed.faces && r.columns.empty(),
        "allocation failure was published or ignored");
  }
  allocator(r,cancel);r.qualification_mesh=std::make_unique<WorldMeshJob>();WorldColumnGpu discarded;
  count(r,*r.qualification_mesh,source,discarded);cancel.wait();
  std::array<std::shared_ptr<WorldMeshAllocation>,WorldMeshAllocator::Capacity-1> queued;
  std::array<rhi::BufferDesc,3> descriptions{};
  for(auto& desc:descriptions) {desc.size=16;desc.elementSize=4;desc.usage=rhi::BufferUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;}
  for(auto& work:queued) {work=r.mesh_allocator->request(descriptions);require(bool(work),"bounded allocation queue rejected a valid slot");}
  require(!r.mesh_allocator->request(descriptions) && r.mesh_allocator->pending()==WorldMeshAllocator::Capacity,
      "allocation queue exceeded fixed capacity");
  std::weak_ptr<WorldMeshAllocation> cancelled=queued.back();queued={};
  require(open_world_renderer_begin_retirement(&r) && !r.qualification_mesh,"retirement joined or retained active mesh allocation");
  require(!r.mesh_allocator->finished() && open_world_renderer_retire_step(&r,32)>0,
      "shutdown hid unfinished allocation ownership");
  require(!cancelled.expired() && open_world_renderer_retirement_frame(&r),
      "cancelled buffers freed early or blocked the closing graphics frame");
  cancel.release();require(r.mesh_allocator->finish(2000000000ull),"cancelled allocator failed to stop");
  drain_retirement(r);
  require(open_world_renderer_retirement_remaining(&r)==0 && !r.mesh_allocator && cancelled.expired(),
      "retirement retained cancelled tickets or ended before resource release");
  require(open_world_renderer_retirement_frame(&r),"final cancelled-resource collection submission");
  fixture.verify("retained_mesh_after_allocator_retirement",source,fixture.read_mesh(output));
  for(const auto& dry:dry_outputs)fixture.verify("shared_fluid_after_allocator_retirement",source,fixture.read_mesh(dry));
  const auto wet_mesh=fixture.read_mesh(wet_output);
  fixture.verify("unique_fluid_after_allocator_retirement",wet,wet_mesh);fixture.verify_fluids(wet,wet_mesh);
  require(r.debug.errors.load()==0,"asynchronous mesh allocation graphics validation errors");
  std::puts("world_mesh_allocation=passed worker_only=1 nonblocking_poll=1 exact_geometry=1 shared_empty_fluid=1 one_fluid_unique=1 bounded_tickets=17 failure=1 cancelled_retirement=1 retained_mesh=1");
}
}
