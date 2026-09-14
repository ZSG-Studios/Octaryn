#include "Probe.h"
#include <slang-rhi/shader-cursor.h>

namespace mesh_probe {
std::vector<std::array<float,4>> sampling_reference(Fixture& f,const std::vector<std::array<float,8>>& inputs) {
  if(inputs.empty())return {};
  auto& r=f.renderer;
  auto input=buffer(r,inputs.data(),inputs.size()*32,16,rhi::BufferUsage::ShaderResource);
  std::vector<std::array<float,4>> output(inputs.size());
  rhi::BufferDesc desc{};desc.size=output.size()*16;desc.elementSize=16;
  desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> result;
  checked(r.device->createBuffer(desc,nullptr,result.writeRef()),"sampling reference result");
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Voxel/SamplingReference.slang","sampling_reference",pipeline),
      "sampling reference production helper");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"sampling reference commands");
  auto* compute=commands->beginComputePass();require(compute!=nullptr,"sampling reference pass");
  auto* root=compute->bindPipeline(pipeline);require(root!=nullptr,"sampling reference root");
  require(bind_world_atlas(r.atlas,root,1),"sampling reference atlas");
  const rhi::ShaderCursor cursor(root);
  checked(cursor["referenceInputs"].setBinding(rhi::Binding(input)),"sampling reference inputs");
  checked(cursor["referenceOutputs"].setBinding(rhi::Binding(result)),"sampling reference output");
  compute->dispatchCompute(static_cast<unsigned>(inputs.size()),1,1);compute->end();
  auto submission=commands->finish();require(submission!=nullptr,"sampling reference finish");
  checked(r.queue->submit(submission),"sampling reference submit");checked(r.queue->waitOnHost(),"sampling reference wait");
  checked(r.device->readBuffer(result,0,desc.size,output.data()),"sampling reference readback");
  return output;
}
}
