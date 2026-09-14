#include "Probe.h"
#include "SlangShaderPath.h"
#include <algorithm>
#include <limits>

namespace mesh_probe {
void patch_coordinate_cases(Fixture& f) {
  std::vector<std::array<unsigned,4>> cases;
  for(unsigned width=1;width<=32;++width)for(unsigned height=1;height<=32;++height)
    for(unsigned ordinal=0;ordinal<std::min(width*height,width+height);++ordinal)
      for(unsigned vertex=0;vertex<6;++vertex)cases.push_back({width,height,ordinal,vertex});
  auto& r=f.renderer;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=resolve_slang_shader_path("octaryn-client/Shaders/Voxel/PatchCoordinates.slang");
  require(create_rhi_compute_pipeline(r.device,path.c_str(),"patch_coordinates",pipeline),"patch coordinate pipeline");
  const auto input=buffer(r,cases.data(),cases.size()*sizeof(cases[0]),16,rhi::BufferUsage::ShaderResource);
  std::vector<unsigned> differences(cases.size(),std::numeric_limits<unsigned>::max());
  rhi::BufferDesc desc{};desc.size=differences.size()*sizeof(unsigned);desc.elementSize=4;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> output;
  checked(r.device->createBuffer(desc,differences.data(),output.writeRef()),"patch coordinate output");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"patch coordinate encoder");
  auto* compute=commands->beginComputePass();require(compute!=nullptr,"patch coordinate pass");
  auto* root=compute->bindPipeline(pipeline);require(root!=nullptr,"patch coordinate binding");
  const auto count=static_cast<unsigned>(cases.size());
  checked(root->setData({0,0,0},&count,sizeof(count)),"patch coordinate case count");
  checked(root->setBinding({0,0,0},rhi::Binding(input)),"patch coordinate cases");
  checked(root->setBinding({0,1,0},rhi::Binding(output)),"patch coordinate differences");
  compute->dispatchCompute((count+63)/64,1,1);compute->end();
  auto submission=commands->finish();require(submission!=nullptr,"patch coordinate command finish");
  checked(r.queue->submit(submission),"patch coordinate submit");checked(r.queue->waitOnHost(),"patch coordinate wait");
  checked(r.device->readBuffer(output,0,desc.size,differences.data()),"patch coordinate readback");
  for(std::size_t i=0;i<differences.size();++i)if(differences[i]!=0) {
    const auto& test=cases[i];
    std::fprintf(stderr,"world_patch_coordinate_difference extent=%ux%u ordinal=%u vertex=%u result=%u\n",
        test[0],test[1],test[2],test[3],differences[i]);
    require(false,"optimized production patch coordinates differ from previous topology");
  }
  require(r.debug.errors.load()==0,"patch coordinate validation errors");
  std::printf("world_patch_coordinates=passed extents=1024 vertices=%u bit_exact=1\n",count);
}
}
