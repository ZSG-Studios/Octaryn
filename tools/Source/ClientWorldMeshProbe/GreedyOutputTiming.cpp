#include "Probe.h"
#include "WorldAtlas.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
namespace mesh_probe {
namespace {
std::uint16_t material(const Fixture& f,const char* name) {
  const auto wanted=std::string("octaryn.basegame.block.")+name;
  for(std::size_t i=1;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return static_cast<std::uint16_t>(i);
  throw std::runtime_error("emit timing fixture material missing");
}
void measure(Fixture& f,const char* name,const StreamColumn& source) {
  constexpr unsigned warmup=4,samples=16,iterations=warmup+samples;
  auto& r=f.renderer;r.sources.clear();auto mesh=f.mesh(source);
  const auto halo=world_mesh_halo(r,source);
  const std::array<std::int32_t,4> info{source.x,source.z,source.min_y,source.height};
  auto blocks=buffer(r,halo.data(),halo.size()*4,4,rhi::BufferUsage::ShaderResource);
  auto metadata=buffer(r,info.data(),sizeof(info),sizeof(info),rhi::BufferUsage::ShaderResource);
  std::array<std::uint32_t,40> arguments{};unsigned face_base=0,patch_base=0;
  for(unsigned pass=0;pass<5;++pass) {
    arguments[pass*4]=6;arguments[pass*4+3]=face_base;face_base+=mesh.gpu.pass_counts[pass];
    arguments[20+pass*4]=6;arguments[23+pass*4]=patch_base;patch_base+=mesh.gpu.patch_counts[pass];
  }
  const auto frequency=r.device->getInfo().timestampFrequency;
  require(frequency!=0,"emit timing requires device timestamps");
  rhi::QueryPoolDesc query_desc{};query_desc.count=iterations*2;query_desc.label="production_mesh_emit_timing";
  Slang::ComPtr<rhi::IQueryPool> queries;
  checked(r.device->createQueryPool(query_desc,queries.writeRef()),"emit timestamp pool");
  checked(queries->reset(),"emit timestamp reset");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"emit timing command encoder");
  for(unsigned iteration=0;iteration<iterations;++iteration) {
    checked(commands->uploadBufferData(mesh.gpu.arguments,0,sizeof(arguments),arguments.data()),"emit counter reset");
    commands->writeTimestamp(queries,iteration*2);
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"emit timing compute pass");
    auto* root=pass->bindPipeline(r.mesh_pipeline);require(root!=nullptr,"production emit pipeline bind");
    const unsigned emit=1;checked(root->setData({0,0,0},&emit,sizeof(emit)),"emit mode binding");
    const std::array<rhi::IBuffer*,7> bindings{blocks,metadata,mesh.gpu.faces,mesh.gpu.arguments,
        world_atlas_materials(r.atlas),mesh.gpu.fluids,mesh.gpu.patches};
    for(unsigned slot=0;slot<bindings.size();++slot)
      checked(root->setBinding({0,slot,0},rhi::Binding(bindings[slot])),"production emit resource binding");
    const unsigned voxels=static_cast<unsigned>(source.blocks.size());
    pass->dispatchCompute(6*32*((static_cast<unsigned>(source.height)+31)/32)+(voxels+31)/32,1,1);
    pass->end();commands->writeTimestamp(queries,iteration*2+1);
    commands->globalBarrier(); // Repeated UAV outputs and the next counter reset are ordered.
  }
  auto submitted=commands->finish();require(bool(submitted),"emit timing finish");
  Slang::ComPtr<rhi::IFence> fence;rhi::FenceDesc fence_desc{};
  checked(r.device->createFence(fence_desc,fence.writeRef()),"emit completion fence");
  rhi::ICommandBuffer* command=submitted;rhi::IFence* completed=fence;const std::uint64_t value=1;
  rhi::SubmitDesc submit{};submit.commandBuffers=&command;submit.commandBufferCount=1;
  submit.signalFences=&completed;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  checked(r.queue->submit(submit),"emit timing submit");
  checked(r.device->waitForFences(1,&completed,&value,true,5000000000ull),"bounded emit timing completion");
  std::array<std::uint64_t,iterations*2> ticks{};
  checked(queries->getResult(0,static_cast<unsigned>(ticks.size()),ticks.data()),"emit timestamp readback");
  std::array<double,samples> milliseconds{};
  for(unsigned i=0;i<samples;++i) {
    const auto begin=ticks[(i+warmup)*2],end=ticks[(i+warmup)*2+1];
    require(end>=begin,"nonmonotonic emit timestamps");
    milliseconds[i]=static_cast<double>(end-begin)*1000.0/static_cast<double>(frequency);
  }
  // Capacity and final data come from the actual retained mesher; every timed
  // dispatch runs its production Slang pipeline, not a copied shader algorithm.
  f.verify(name,source,f.read_mesh(mesh.gpu));
  const double mean=std::accumulate(milliseconds.begin(),milliseconds.end(),0.0)/samples;
  std::sort(milliseconds.begin(),milliseconds.end());
  std::printf("world_mesh_emit_timing scene=%s warmup=%u samples=%u rectangles=%u patches=%u mean_ms=%.6f median_ms=%.6f max_ms=%.6f validation=enabled\n",
      name,warmup,samples,face_base,patch_base,mean,(milliseconds[7]+milliseconds[8])*.5,milliseconds.back());
}
}
void greedy_output_timing(Fixture& f) {
  const auto stone=material(f,"stone"),grass=material(f,"grass");
  auto solid=column(-2,3,-32,32);solid.blocks.fill(stone);measure(f,"solid32",solid);
  auto checker=column(-2,3,-32,32);
  for(int z=0;z<32;++z)for(int y=0;y<32;++y)for(int x=0;x<32;++x)put(checker,x,y,z,((x+y+z)&1)?grass:stone);
  measure(f,"material_checker32",checker);
  auto stepped=column(-2,3,-32,64);
  for(int z=0;z<32;++z)for(int x=0;x<32;++x)for(int y=0;y<16+(x/4+z/4)*3;++y)
    put(stepped,x,y,z,stone);
  measure(f,"stepped64",stepped);f.renderer.sources.clear();
}
}
