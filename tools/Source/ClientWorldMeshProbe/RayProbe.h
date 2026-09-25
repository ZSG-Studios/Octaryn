#pragma once
#include "Probe.h"
#include "ResourceProbePacing.h"
#include "SlangShaderPath.h"
#include "WorldRayTracingState.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace mesh_probe {
namespace {
struct Ray {std::array<float,4> origin,direction;};
struct Result {
  std::array<std::uint32_t,4> identity;
  std::array<float,4> distance_normal,position,albedo,visibility;
};
static_assert(sizeof(Ray)==32 && sizeof(Result)==80);
constexpr unsigned Count=7;
// A signed-world cube occupies [-49,-48] x [-17,-16] x [111,112].
constexpr std::array<Ray,Count> Rays{{
    {{-48.5f,-10,111.5f,30},{0,-1,0,0}},
    {{-55,-16.5f,111.5f,8},{1,0,0,0}},
    {{-48.5f,-22,111.5f,30},{0,1,0,0}},
    {{-47.5f,-10,111.5f,30},{0,-1,0,0}},
    {{-48.5f,-10,111.5f,5},{0,-1,0,0}},
    {{-48.5f,-10,111.5f,30},{0,1,0,0}},
    {{-44.5f,-10,111.5f,30},{0,-1,0,0}}
}};
using Results=std::vector<Result>;
class RayProbePipeline {
  WorldRenderer& renderer;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  static inline RayProbePipeline* active{};
public:
  explicit RayProbePipeline(WorldRenderer& r):renderer(r) {
    require(!active,"ray probe setup already active");
    const auto path=resolve_slang_shader_path("octaryn-client/Shaders/RayTracing/RayTracingProbe.slang");
    require(create_rhi_compute_pipeline(r.device,path.c_str(),"main",pipeline),"production ray query probe pipeline");
    active=this;
  }
  ~RayProbePipeline() {active=nullptr;}
  static rhi::IComputePipeline* get(WorldRenderer& r) {
    require(active && &active->renderer==&r,"ray probe pipeline owner mismatch");return active->pipeline;
  }
};
class Probe {
  WorldRenderer& r;
  WorldFrames frames;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> input,lights;
  unsigned ray_count{};
  std::array<Slang::ComPtr<rhi::IBuffer>,2> output;
  std::uint64_t serial{};
public:
  explicit Probe(WorldRenderer& renderer,std::span<const Ray> rays=Rays):r(renderer),ray_count(static_cast<unsigned>(rays.size())) {
    require(frames.initialize(r.device,2),"ray probe two frame slots");
    require(r.capabilities.inline_lighting(),"central capabilities rejected the required ray-query device");
    require(world_ray_available(r),"ray probe requires prepared hardware ray queries");
    if(const auto* budget=std::getenv("OCTARYN_CLIENT_MESH_PROBE_RAY_BUILD_BUDGET");budget && std::strcmp(budget,"2")==0) {
      world_ray_set_build_budget(r,2,262144);
      std::puts("world_ray_probe_control builds_per_frame=2 face_budget=262144");
    }
    pipeline=RayProbePipeline::get(r);
    input=buffer(r,rays.data(),rays.size_bytes(),sizeof(Ray),rhi::BufferUsage::ShaderResource);
    std::vector<WorldLocalLight> sources(rays.size());
    for(std::size_t i=0;i<rays.size();++i)for(unsigned axis=0;axis<3;++axis)
      sources[i].position_range[axis]=rays[i].origin[axis]+rays[i].direction[axis]*rays[i].origin[3];
    lights=buffer(r,sources.data(),sources.size()*sizeof(WorldLocalLight),sizeof(WorldLocalLight),rhi::BufferUsage::ShaderResource);
    rhi::BufferDesc desc{};desc.size=ray_count*sizeof(Result);desc.elementSize=sizeof(Result);
    desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
    desc.defaultState=rhi::ResourceState::UnorderedAccess;
    for(auto& result:output)checked(r.device->createBuffer(desc,nullptr,result.writeRef()),"ray probe result creation");
  }
  ~Probe() {frames.drain();r.queue->waitOnHost();}
  unsigned submit(const std::shared_ptr<world_ray::Snapshot>& retained={},bool prepare=true) {
    require(prepare || bool(retained),"raw ray query requires a retained immutable scene");
    resource_probe::admit();
    const auto slot=frames.slot(serial++);require(frames.wait(slot),"ray probe frame slot wait");
    r.active_frame=slot;r.frames=serial;
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"ray probe command encoder");
    if(prepare) {
      require(prepare_player_shadows(r.player,commands,slot,r.player_pose,true),"production player shadow prepare");
      require(world_ray_prepare(r,commands,slot),"production ray scene prepare");
    }
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"ray probe compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"ray probe pipeline binding");
    require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"production ray scene and atlas bindings");
    const rhi::ShaderCursor cursor(root);
    if(retained) {
      checked(cursor["rayScene"].setBinding(rhi::Binding(retained->tlas)),"retained TLAS binding");
      checked(cursor["rayRecords"].setBinding(rhi::Binding(retained->records)),"retained records binding");
    }
    checked(cursor["localLights"].setBinding(rhi::Binding(lights)),"ray local lights binding");
    checked(cursor["probeRays"].setBinding(rhi::Binding(input)),"ray input binding");
    checked(cursor["probeResults"].setBinding(rhi::Binding(output[slot])),"ray output binding");
    pass->dispatchCompute(ray_count,1,1);pass->end();
    auto command=commands->finish();require(command!=nullptr,"ray probe command finish");
    require(frames.submit(r.queue,command,slot),"ray probe frame submit");return slot;
  }
  Results read(unsigned slot) {
    require(frames.wait(slot),"ray probe readback frame wait");
    Results result(ray_count);
    checked(r.device->readBuffer(output[slot],0,result.size()*sizeof(Result),result.data()),"ray probe result readback");
    for(const auto& hit:result) {
      require(hit.identity[0]<=1,"ray hit flag invalid");
      for(const auto& values:{hit.distance_normal,hit.position,hit.albedo,hit.visibility})
        for(const auto value:values)require(std::isfinite(value),"ray query produced non-finite result");
    }
    resource_probe::complete("ray_readback");return result;
  }
  Results settle(unsigned expected) {
    for(unsigned iteration=0;iteration<64;++iteration) {
      const auto result=read(submit());const auto stats=world_ray_stats(r);
      require(stats.active_jobs<=world_ray::BuildJobCapacity,"ray BLAS build job bound exceeded");
      if(stats.ready_columns==expected && !stats.pending_columns && !stats.active_jobs)return result;
      // Qualification-only wait makes every next production zero-time poll deterministic.
      finish_builds();
    }
    require(false,"ray scene did not settle within bounded fixture iterations");return {};
  }
  void finish_builds() {
    // Qualification-only waits; production allocation polling never waits.
    auto& state=*r.ray_tracing->state;
    for(auto& job:state.jobs)if(job.allocation)
      require(job.allocation->wait(2000000000ull),"ray probe allocation completion");
    for(unsigned pass=0;pass<world_ray::BuildJobCapacity;++pass) {
      resource_probe::Frame frame("ray_build_progress");
      require(state.poll(r),"ray probe allocation admission");
      checked(r.queue->waitOnHost(),"ray probe build progress wait");
      if(std::none_of(state.jobs.begin(),state.jobs.end(),[](const auto& job){return bool(job.allocation);}))break;
    }
  }
};
}
}
