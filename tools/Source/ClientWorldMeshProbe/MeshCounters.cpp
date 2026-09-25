#include "Probe.h"
#include "ResourceProbePacing.h"
#include "WorldMeshJob.h"

namespace mesh_probe {
namespace {
std::array<std::uint16_t,5> materials(const Fixture& f) {
  constexpr std::array<const char*,5> names{"stone","bush","glass","water","lava"};
  std::array<std::uint16_t,5> result{};
  for(unsigned p=0;p<names.size();++p) {
    const auto wanted=std::string("octaryn.basegame.block.")+names[p];
    for(unsigned id=1;id<f.catalog.size();++id)
      if(f.catalog[id].id==wanted)result[p]=static_cast<std::uint16_t>(id);
    require(result[p]!=0 && pass(f.catalog[result[p]])==p,"counter fixture named material missing or wrong pass");
    if(p>=3)require(f.catalog[result[p]].fluidLevel==0,"counter fixture needs source water/lava");
  }
  return result;
}
Mesh finish(Fixture& f,WorldMeshJob& job,const StreamColumn& source) {
  auto& r=f.renderer;WorldColumnGpu output;bool completed{};
  {
    resource_probe::Frame frame("mesh_counter_count");
    require(job.start(r,source),"counter reuse count start");
    require(!job.finished_counters(),"new job exposed previous emitted counters");
    require(job.wait(1000000000ull),"counter reuse count bounded completion");
    require(!job.finished_counters(),"count fence exposed emit counters");
    require(job.poll(r,output,completed) && !completed && job.resources().allocating,
        "counter fixture count did not enter private allocation");
    require(!job.finished_counters(),"allocation phase exposed emit counters");
  }
  {
    resource_probe::Frame frame("mesh_counter_emit");
    require(job.wait(1000000000ull),"counter reuse allocation bounded completion");
    require(job.poll(r,output,completed) && !completed && job.resources().emitting,
        "counter fixture allocation did not submit emit");
    require(!job.finished_counters(),"unobserved emit exposed scratch counters");
    require(job.wait(1000000000ull),"counter reuse emit bounded completion");
    require(!job.finished_counters(),"emit wait bypassed owner observation");
    require(job.poll(r,output,completed) && completed,"counter reuse emit publication");
  }
  resource_probe::Frame frame("mesh_counter_readback");
  auto* counters=job.finished_counters();
  require(counters!=nullptr && counters->getDesc().size==160,"finished mesh did not expose exact 40-word scratch");
  const auto mesh=f.read_mesh(output,counters);
  require(mesh.counters_read,"GPU emitted words were not inspected");
  f.verify("reused_job_gpu_counters",source,mesh);
  return mesh;
}
void same_retained(Fixture& f,const StreamColumn& source,const Mesh& saved) {
  resource_probe::Frame frame("mesh_counter_retained");
  const auto current=f.read_mesh(saved.gpu);
  require(!current.counters_read,"retained mesh unexpectedly relies on job scratch");
  require(current.faces==saved.faces && current.patches==saved.patches && current.fluids==saved.fluids,
      "reusing scratch counters overwrote retained mesh output");
  require(current.gpu.pass_counts==saved.gpu.pass_counts && current.gpu.patch_counts==saved.gpu.patch_counts,
      "reusing scratch counters mutated retained draw ranges");
  f.verify("retained_output_after_counter_reuse",source,current);
}
}
void mesh_counters_cases(Fixture& f) {
  // The raster fixture warms all pipelines before starting continuous capped
  // supervision. It never restarts the completed-work timing interval.
  mesh_draw_counter_parity(f);
  auto& r=f.renderer;r.sources.clear();r.columns.clear();
  const auto ids=materials(f);
  std::array<StreamColumn,3> sources{column(-1,-1,0,8),column(-1,-1,0,8),column(-1,-1,0,8)};
  for(unsigned p=0;p<5;++p)put(sources[0],2+int(p)*6,4,6,ids[p]);
  put(sources[1],3,2,4,ids[0]);put(sources[1],10,2,4,ids[0]);
  std::array<Mesh,3> retained;
  {
    WorldMeshJob job;require(!job.finished_counters(),"unused job exposed counters");
    rhi::IBuffer* original_counter_identity{};
    for(unsigned cycle=0;cycle<sources.size();++cycle) {
      r.sources.insert_or_assign({-1,-1},sources[cycle]);
      {
        resource_probe::Frame frame("mesh_counter_cpu_fixture");
        constexpr std::array<std::array<unsigned,5>,3> expected{{{6,4,6,6,6},{12,0,0,0,0},{0,0,0,0,0}}};
        std::array<unsigned,5> counts{};
        for(const auto& face:f.expected(sources[cycle]))++counts[pass(f.catalog[face[3]&65535])];
        require(counts==expected[cycle],"counter fixture CPU oracle does not cover the requested material passes");
        std::printf("world_mesh_counter_fixture cycle=%u cpu_pass_faces=%u,%u,%u,%u,%u\n",
            cycle,counts[0],counts[1],counts[2],counts[3],counts[4]);
      }
      retained[cycle]=finish(f,job,sources[cycle]);
      auto* identity=job.finished_counters();
      if(!cycle)original_counter_identity=identity;
      else require(identity==original_counter_identity,"same-sized job reallocated reusable counters");
      for(unsigned p=0;p<5;++p) {
        const bool present=cycle==0 || (cycle==1 && p==0);
        require((retained[cycle].gpu.pass_counts[p]!=0)==present &&
            (retained[cycle].gpu.patch_counts[p]!=0)==present,"reused count/emit retained a cleared material range");
      }
      if(cycle)require(retained[cycle].counters!=retained[cycle-1].counters,"counter fixture did not change emitted words");
      for(unsigned old=0;old<cycle;++old)same_retained(f,sources[old],retained[old]);
    }
    require(job.resources().fences_created==1,"counter reuse recreated its completion fence");
  }
  for(unsigned old=0;old<retained.size();++old)same_retained(f,sources[old],retained[old]);
  require(r.debug.errors.load()==0,"counter lifetime graphics validation errors");
  std::puts("world_mesh_counters=passed gpu_words=40 cycles=3 all_passes=5 before_observed_emit_hidden=1 same_scratch=1 empty_reset=1 retained_after_job_destroy=1");
}
}
