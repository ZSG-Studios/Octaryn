#include "Probe.h"
#include "FrameWatchdog.h"
#include "ResourceProbePacing.h"
#include "RetirementDrain.h"
#include "WorldRayTracingState.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace mesh_probe {
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
    require(changed.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}),"ray allocation callback did not start");
  }
  static SlangResult create(void* context,const rhi::AccelerationStructureDesc& desc,rhi::IAccelerationStructure** out) {
    auto& gate=*static_cast<Gate*>(context);std::unique_lock lock(gate.mutex);
    gate.worker=std::this_thread::get_id();gate.entered=true;gate.changed.notify_all();
    if(!gate.changed.wait_for(lock,std::chrono::seconds(5),[&]{return gate.released;}))return SLANG_FAIL;
    if(gate.fail)return SLANG_FAIL;
    lock.unlock();return gate.device->createAccelerationStructure(desc,out);
  }
};
struct Cleanup {
  WorldRenderer& renderer;std::array<Gate*,4> gates;
  ~Cleanup() {
    for(auto* gate:gates)gate->release();
    if(renderer.ray_tracing && renderer.ray_tracing->state->allocator) {
      auto& allocator=renderer.ray_tracing->state->allocator;
      allocator->stop();
      if(!allocator->finish(2000000000ull))frame_gpu_shutdown_failed("ray_probe_allocator_cleanup");
    }
  }
};
void replace(WorldRenderer& r,Gate& gate) {
  auto& state=*r.ray_tracing->state;
  if(state.allocator) {
    state.allocator->stop();require(state.allocator->finish(2000000000ull),"previous ray allocator shutdown");
  }
  checked(r.queue->waitOnHost(),"ray allocation fixture completed GPU work");
  for(auto& job:state.jobs) {
    job.allocation.reset();job.pending.reset();job.submission.setNull();job.signal=0;
  }
  state.allocator=std::make_unique<WorldRayAllocator>(r.device,Gate::create,&gate);
}
}
void ray_allocation_cases(Fixture& fixture) {
  resource_budget_cases();
  auto& r=fixture.renderer;
  require(world_ray_initialize(r) && world_ray_available(r),"ray allocation fixture requires hardware ray queries");
  auto& state=*r.ray_tracing->state;
  Gate ready(r.device),edited(r.device),failure(r.device),cancel(r.device);
  Cleanup cleanup{r,{&ready,&edited,&failure,&cancel}};
  unsigned stone{};
  for(unsigned i=0;i<fixture.catalog.size();++i)if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"ray allocation stone fixture missing");
  auto source=column();put(source,5,7,9,static_cast<std::uint16_t>(stone));
  const auto mesh=fixture.mesh(source);r.columns[{0,0}]=mesh.gpu;
  resource_probe::start();
  replace(r,ready);
  require(state.start(r,{0,0},r.columns.at({0,0})),"asynchronous BLAS admission failed");ready.wait();
  auto& first=state.jobs[0];
  require(first.pending && first.allocation && !first.pending->blas && first.signal==0 &&
      ready.worker!=std::this_thread::get_id(),"BLAS allocation did not remain private on worker");
  for(unsigned iteration=0;iteration<16;++iteration) {
    resource_probe::Frame frame("blocked_ray_owner_progress");
    require(state.poll(r) && state.columns.empty() && first.signal==0,"pending BLAS poll waited or published early");
    require(r.frame_queue.synchronize(r.queue,1000),"blocked ray allocation prevented real graphics completion");
  }
  resource_probe::admit();
  state.refresh_bytes(r);
  require(state.stats.temporary_bytes>0,"pending AABB allocation missing from resident resource accounting");
  require(r.frame_queue.synchronize(r.queue,1000),"graphics owner blocked behind BLAS allocation worker");
  ready.release();require(first.allocation->wait(2000000000ull),"BLAS allocation did not complete");
  require(state.poll(r) && !first.allocation && first.signal!=0 && state.columns.empty(),
      "ready BLAS did not submit privately before fence publication");
  checked(r.queue->waitOnHost(),"asynchronous BLAS build completion");
  require(state.poll(r) && !first.pending && state.columns.at({0,0})->matches(mesh.gpu),
      "completed asynchronous BLAS lost exact mesh identity");
  const auto retained=state.columns.at({0,0});
  require(retained->blas && state.allocator->created()==1,"actual BLAS allocation count missing");
  resource_probe::complete("ray_allocation_build");
  const auto original_builds=state.stats.blas_builds;
  resource_probe::admit();
  replace(r,edited);r.columns[{1,0}]=mesh.gpu;
  require(state.start(r,{1,0},r.columns.at({1,0})),"edited allocation fixture admission");edited.wait();
  auto pending=state.jobs[0].allocation;std::weak_ptr<WorldRayAllocation> discarded=pending;
  auto next=source;put(next,5,7,9,0);put(next,8,7,9,static_cast<std::uint16_t>(stone));
  r.columns[{1,0}]=fixture.mesh(next).gpu;
  require(state.poll(r) && pending->cancelled.load() && state.jobs[0].pending,
      "edit did not cancel private allocation while preserving worker ownership");
  edited.release();require(pending->wait(2000000000ull),"cancelled BLAS allocation completion");
  require(state.poll(r) && !state.jobs[0].pending && !state.columns.contains({1,0}) &&
      state.stats.blas_builds==original_builds,"stale allocation reached the GPU build or scene");
  pending.reset();state.allocator->stop();require(state.allocator->finish(2000000000ull),"edited worker stop");
  require(discarded.expired(),"cancelled allocation ticket survived owner collection");
  resource_probe::complete("ray_edit_cancellation");
  resource_probe::admit();
  replace(r,failure);failure.fail=true;
  require(state.start(r,{1,0},r.columns.at({1,0})),"failed allocation fixture admission");failure.wait();failure.release();
  require(state.jobs[0].allocation->wait(2000000000ull) && !state.poll(r) && !state.columns.contains({1,0}),
      "allocation error was ignored or published");
  resource_probe::complete("ray_allocation_failure");
  resource_probe::admit();
  replace(r,cancel);
  for(unsigned i=0;i<WorldRayAllocator::Capacity;++i) {
    const world_ray::Coord coordinate{int(i+2),0};r.columns[coordinate]=mesh.gpu;
    require(state.start(r,coordinate,r.columns.at(coordinate)),"bounded ray allocation admission");
  }
  cancel.wait();
  require(state.allocator->pending()==WorldRayAllocator::Capacity && !state.allocator->request(6,nullptr,nullptr),
      "ray allocation queue exceeded eight tickets");
  std::weak_ptr<WorldRayAllocation> cancelled=state.jobs.back().allocation;
  require(open_world_renderer_begin_retirement(&r) && !state.jobs[0].pending && !state.jobs[0].allocation,
      "retirement retained jobs or joined blocked allocation");
  require(!state.allocator->finished() && open_world_renderer_retire_step(&r,32)>0 && !cancelled.expired(),
      "closing ended before cancelled allocation ownership finished");
  require(open_world_renderer_retirement_frame(&r),"blocked allocation prevented real closing submission");
  resource_probe::complete("ray_cancel_closing");
  cancel.release();require(state.allocator->finish(2000000000ull),"cancelled ray allocator failed to finish");
  drain_retirement(r);
  require(!state.allocator && cancelled.expired() && r.columns.empty() && state.columns.empty(),
      "ray retirement retained allocator or world owners");
  resource_probe::admit();
  require(open_world_renderer_retirement_frame(&r),"ray retirement final GPU maintenance");
  require(retained->blas && retained->matches(mesh.gpu) && r.debug.errors.load()==0,
      "retained immutable BLAS lost resources or failed validation");
  resource_probe::complete("ray_retirement_complete");
  std::puts("world_ray_allocation=passed worker_only=1 nonblocking_poll=1 exact_mesh_identity=1 fence_publication=1 "
      "edit_cancel=1 failure=1 bounded_tickets=8 cancelled_retirement=1 retained_blas=1 validation_errors=0");
}
}
