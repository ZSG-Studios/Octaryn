#include "Probe.h"
#include "RayProbe.h"
#include "AtlasInternal.h"
#include "FrameWatchdog.h"
#include "RetirementDrain.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace mesh_probe {
namespace {
struct AllocationGate {
  rhi::IDevice* device;
  std::mutex mutex;
  std::condition_variable changed;
  unsigned entered{},allowed{},fail_at{};
  std::thread::id worker;
  explicit AllocationGate(rhi::IDevice* value):device(value) {}
  void allow(unsigned count) {std::lock_guard lock(mutex);allowed=count;changed.notify_all();}
  void fail(unsigned ordinal) {std::lock_guard lock(mutex);fail_at=ordinal;}
  void await(unsigned count) {
    std::unique_lock lock(mutex);
    require(changed.wait_for(lock,std::chrono::seconds(2),[&]{return entered>=count;}),
        "private BLAS allocation gate was not reached");
  }
  static SlangResult create(void* context,const rhi::AccelerationStructureDesc& desc,rhi::IAccelerationStructure** output) {
    auto& gate=*static_cast<AllocationGate*>(context);std::unique_lock lock(gate.mutex);
    gate.worker=std::this_thread::get_id();const auto ordinal=++gate.entered;gate.changed.notify_all();
    if(!gate.changed.wait_for(lock,std::chrono::seconds(5),[&]{return gate.allowed>=ordinal;}))return SLANG_FAIL;
    if(ordinal==gate.fail_at)return SLANG_FAIL;
    lock.unlock();return gate.device->createAccelerationStructure(desc,output);
  }
};
struct Cleanup {
  WorldRenderer& renderer;
  AllocationGate& gate;
  ~Cleanup() {
    gate.allow(UINT32_MAX);
    if(renderer.ray_tracing && renderer.ray_tracing->state->allocator) {
      auto& allocator=renderer.ray_tracing->state->allocator;allocator->stop();
      if(!allocator->finish(2000000000ull))frame_gpu_shutdown_failed("private_ray_probe_allocator_cleanup");
    }
  }
};
struct Scene {
  std::uint64_t generation,journal;
  std::uint32_t ready,pending;
  std::shared_ptr<world_ray::Snapshot> current;
  std::array<std::shared_ptr<world_ray::Snapshot>,2> frames;
  std::map<world_ray::Coord,std::shared_ptr<world_ray::Column>> columns,changed;
  std::map<world_ray::Coord,std::array<std::uint32_t,5>> pass_counts;
  explicit Scene(const WorldRenderer& r) {
    const auto& s=*r.ray_tracing->state;
    generation=s.generation;journal=r.scene_changes.revision();ready=s.stats.ready_columns;pending=s.stats.pending_columns;
    current=s.current;frames={s.frames[0].snapshot,s.frames[1].snapshot};
    columns=s.columns;changed=s.changed;pass_counts=s.built_pass_counts;
  }
  void unchanged(const WorldRenderer& r) const {
    const auto& s=*r.ray_tracing->state;
    require(generation==s.generation && journal==r.scene_changes.revision() && ready==s.stats.ready_columns &&
        pending==s.stats.pending_columns && current==s.current && frames[0]==s.frames[0].snapshot &&
        frames[1]==s.frames[1].snapshot && columns==s.columns && changed==s.changed && pass_counts==s.built_pass_counts,
        "private BLAS work published or mutated visible ray scene state");
  }
};
unsigned active(const WorldRayTracing::State& s) {
  return static_cast<unsigned>(std::count_if(s.jobs.begin(),s.jobs.end(),[](const auto& job){return bool(job.pending);}));
}
world_ray::BuildJob& job(WorldRayTracing::State& s,int x) {
  const auto found=std::find_if(s.jobs.begin(),s.jobs.end(),[&](const auto& value){return value.pending && value.coordinate==world_ray::Coord{x,0};});
  require(found!=s.jobs.end(),"private BLAS fixture job missing");return *found;
}
void allocation_ready(WorldRayTracing::State& s,int x) {
  auto& work=job(s,x);require(work.allocation && work.allocation->wait(2000000000ull),"private BLAS ticket did not complete");
}
void query(Probe& probe,const std::shared_ptr<world_ray::Snapshot>& scene,unsigned stone,unsigned columns) {
  // No prepare/poll call: real GPU requery must not advance the scene under test.
  const auto result=probe.read(probe.submit(scene,false));
  for(unsigned ray=0;ray<result.size();++ray) {
    const bool expected=ray<columns;
    require((result[ray].identity[0]!=0)==expected,"private-stage ray query disagrees with published cube set");
    if(expected) {
      require(result[ray].identity[1]==stone && std::abs(result[ray].distance_normal[0]-6.f)<.001f,
          "private-stage query material or distance differs from cube oracle");
      require(std::abs(result[ray].distance_normal[1])<.001f &&
          std::abs(result[ray].distance_normal[2]-1.f)<.001f && std::abs(result[ray].distance_normal[3])<.001f,
          "private-stage query normal differs from cube oracle");
    }
  }
}
void progress(WorldRenderer& r,const Scene& scene,bool isolate_cpu=false,unsigned slices=16) {
  for(unsigned slice=0;slice<slices;++slice) {
    // Qualification-only: count/face cases must not pass due to slow driver CPU work.
    if(isolate_cpu)r.ray_tracing->state->submission_budget.cpu_ns=0;
    require(world_ray_progress(r,.5),"private BLAS advancement failed");
    scene.unchanged(r);require(active(*r.ray_tracing->state)<=world_ray::BuildJobCapacity,"private work exceeded eight BLAS slots");
  }
}
}
void ray_private_progress_cases(Fixture& fixture) {
  auto& r=fixture.renderer;
  RayProbePipeline pipeline(r);
  require(world_ray_initialize(r) && world_ray_available(r),"private BLAS fixture requires hardware ray queries");
  require(prepare_world_atlas_plant_masks(r.atlas),"private BLAS prewarm atlas masks");
  unsigned stone{};
  for(unsigned i=0;i<fixture.catalog.size();++i)if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"private BLAS fixture stone missing");
  open_world_renderer_set_center(&r,-1,0,16);
  std::array<WorldColumnGpu,15> meshes;
  std::vector<Ray> rays;
  for(int x=-1;x<14;++x) {
    auto source=column(x,0,0,32);put(source,15,15,15,static_cast<std::uint16_t>(stone));
    meshes[static_cast<unsigned>(x+1)]=fixture.mesh(source).gpu;
    rays.push_back({{float(x*32)+15.5f,22,15.5f,30},{0,-1,0,0}});
  }
  r.columns[{-1,0}]=meshes[0];
  Probe probe(r,rays);world_ray_set_build_budget(r,8,48);probe.settle(1);
  auto& state=*r.ray_tracing->state;
  const auto held=state.current;
  const auto held_generation=held->generation;
  auto* held_tlas=held->tlas.get();auto* held_records=held->records.get();
  AllocationGate gate(r.device);Cleanup cleanup{r,gate};
  state.allocator->stop();require(state.allocator->finish(2000000000ull),"private fixture prewarm allocator stop");
  state.allocator=std::make_unique<WorldRayAllocator>(r.device,AllocationGate::create,&gate);
  resource_probe::start();
  for(int x=0;x<8;++x)r.columns[{x,0}]=meshes[static_cast<unsigned>(x+1)];
  {
    for(unsigned frame=0;frame<world_ray::BuildJobCapacity && active(state)<8;++frame)
      probe.read(probe.submit());
    gate.await(1);
    require(active(state)==8 && state.allocator->pending()==8 && state.stats.ready_columns==1,
        "bounded frame heads did not admit all eight private BLAS slots");
    const Scene blocked(r);const auto builds=state.stats.blas_builds;
    const auto tickets=state.allocator->pending();
    progress(r,blocked);
    require(state.stats.blas_builds==builds && state.allocator->pending()==tickets &&
        gate.worker!=std::this_thread::get_id(),"blocked private work waited, submitted or requested new allocations");
    query(probe,held,stone,1);blocked.unchanged(r);
  }
  gate.allow(1);allocation_ready(state,0);gate.await(2);
  world_ray_set_build_budget(r,2,48);
  const auto mixed_before=state.stats.blas_builds;
  probe.read(probe.submit());
  require(state.stats.blas_builds==mixed_before+1 && job(state,0).signal!=0,
      "shared-budget head failed to submit exactly the first ready cube");
  gate.allow(2);allocation_ready(state,1);gate.await(3);
  {
    const Scene mixed(r);const auto allocations=state.allocator->created();
    ++r.frames; // Production increments this after render, before cap work.
    progress(r,mixed,true);
    require(state.stats.blas_builds==mixed_before+2 && state.stats.blas_private_submissions==1 &&
        state.submission_budget.work.started==2 && state.submission_budget.work.faces==12 &&
        state.allocator->created()==allocations && active(state)==8,
        "head plus repeated private slices exceeded shared count/faces or admitted new work");
    gate.allow(3);allocation_ready(state,2);gate.await(4);
    require(r.frame_queue.synchronize(r.queue,2000),"private BLAS builds did not complete");
    progress(r,mixed,true);query(probe,held,stone,1);mixed.unchanged(r);
    progress(r,mixed,true);
    require(state.stats.blas_builds==mixed_before+2 && job(state,2).signal==0 && job(state,2).allocation,
        "ready third ticket bypassed shared head/private count after a frame-counter increment");
  }
  world_ray_set_build_budget(r,8,48);
  probe.read(probe.submit());
  require(state.stats.ready_columns==3 && state.stats.blas_builds==mixed_before+3,
      "next frame head did not publish two private builds and submit the next ready ticket");
  {
    const Scene timed(r);const auto builds=state.stats.blas_builds;
    // Deterministic clock injection tests the production budget, not a sleep race.
    require(state.submission_budget.work.started==1,"CPU budget fixture lacks prior head submission");
    state.submission_budget.cpu_ns=world_ray::BuildCpuBudgetNs;
    gate.allow(8);
    for(int x=3;x<8;++x)allocation_ready(state,x);
    progress(r,timed);
    require(state.stats.blas_builds==builds && state.submission_budget.work.started==1,
        "private slices reset elapsed head CPU budget with ready tickets remaining");
    query(probe,state.current,stone,3);timed.unchanged(r);
  }
  probe.settle(9);query(probe,state.current,stone,9);query(probe,held,stone,1);
  require(state.allocator->created()==8 && !active(state) && held->generation==held_generation &&
      held->tlas.get()==held_tlas && held->records.get()==held_records,
      "eight-slot completion changed allocation count or retained snapshot identity");
  // Two existing tickets cross a frame boundary before the face limit changes.
  for(int x=8;x<10;++x)r.columns[{x,0}]=meshes[static_cast<unsigned>(x+1)];
  for(unsigned frame=0;frame<2 && active(state)<2;++frame)probe.read(probe.submit());
  require(active(state)==2,"face fixture did not admit both blocked tickets");
  gate.await(9);gate.allow(10);
  allocation_ready(state,8);allocation_ready(state,9);
  world_ray_set_build_budget(r,8,6);
  const auto face_before=state.stats.blas_builds;probe.read(probe.submit());
  {
    const Scene faces(r);progress(r,faces,true);
    require(state.stats.blas_builds==face_before+1 && state.submission_budget.work.started==1 &&
        state.submission_budget.work.faces==6,"private slices reset head face budget");
    query(probe,state.current,stone,9);faces.unchanged(r);
  }
  world_ray_set_build_budget(r,8,48);probe.settle(11);query(probe,state.current,stone,11);
  r.columns[{10,0}]=meshes[11];probe.read(probe.submit());gate.await(11);
  {
    const Scene cancelled(r);auto ticket=job(state,10).allocation;const auto builds=state.stats.blas_builds;
    r.columns.erase({10,0});progress(r,cancelled);
    require(ticket->cancelled.load() && active(state)==1,"private stale-source cancellation lost worker ownership");
    gate.allow(11);require(ticket->wait(2000000000ull),"cancelled private ticket failed to finish");
    progress(r,cancelled);
    require(!active(state) && state.stats.blas_builds==builds,"cancelled ticket reached GPU submission");
    query(probe,state.current,stone,11);cancelled.unchanged(r);
  }
  r.columns[{11,0}]=meshes[12];probe.read(probe.submit());gate.await(12);gate.fail(12);gate.allow(12);
  allocation_ready(state,11);
  {
    const Scene failed(r);const auto builds=state.stats.blas_builds;
    require(!world_ray_progress(r,.5),"private non-cancelled allocation failure was swallowed");
    failed.unchanged(r);require(state.stats.blas_builds==builds,"failed allocation submitted a GPU build");
    // Fixture cleanup explicitly makes the failed source stale; normal callers stop.
    r.columns.erase({11,0});progress(r,failed);
    require(!active(state),"failed ticket remained owned after explicit fixture cancellation");
    query(probe,held,stone,1);failed.unchanged(r);
  }
  r.columns[{12,0}]=meshes[13];r.columns[{13,0}]=meshes[14];
  for(unsigned frame=0;frame<2 && active(state)<2;++frame)probe.read(probe.submit());
  require(active(state)==2,"retirement fixture did not admit both blocked tickets");
  gate.await(13);gate.allow(13);allocation_ready(state,12);gate.await(14);
  const std::weak_ptr<WorldRayAllocation> closing_ticket=job(state,13).allocation;
  {
    const Scene closing(r);progress(r,closing);
    require(job(state,12).signal!=0 && job(state,13).allocation && active(state)==2,
        "retirement fixture lacks simultaneous submitted and blocked private jobs");
    query(probe,held,stone,1);closing.unchanged(r);
    resource_probe::Frame frame("private_ray_retirement");
    require(open_world_renderer_begin_retirement(&r) && !active(state) && !state.allocator->finished() &&
        !closing_ticket.expired(),"retirement lost blocked worker ownership or retained pending jobs");
    require(open_world_renderer_retirement_frame(&r),"private BLAS retirement maintenance failed");
  }
  gate.allow(UINT32_MAX);require(state.allocator->finish(2000000000ull),"private BLAS closing worker failed to stop");
  drain_retirement(r);
  require(!state.allocator && closing_ticket.expired() && r.columns.empty() && state.columns.empty() &&
      r.debug.errors.load()==0,"private BLAS retirement left owners or validation errors");
  std::puts("world_ray_private_progress=passed slots=8 late_completion=1 owner_submission=1 private_fence_completion=1 "
      "head_publication=1 retained_scene_requery=1 shared_count=1 shared_faces=1 count_face_cpu_isolated=1 shared_cpu_fake_clock=1 "
      "frame_counter_increment=1 cancelled=1 allocation_failure=1 submitted_and_blocked_retirement=1 validation_errors=0");
}
}
