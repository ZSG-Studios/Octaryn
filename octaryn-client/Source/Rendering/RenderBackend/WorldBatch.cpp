#include "WorldRendererInternal.h"
#include "WorldBatch.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstring>

namespace octaryn::client::rendering {
WorldBatch::~WorldBatch() {
  for(auto& frame:frames) {
    if(device && frame.mapped_records) device->unmapBuffer(frame.records);
    if(device && frame.mapped_arguments) device->unmapBuffer(frame.arguments);
  }
}
std::uint64_t WorldBatch::gpu_bytes() const {
  std::uint64_t bytes{};
  for(const auto& frame:frames) {
    if(frame.records)bytes+=frame.records->getDesc().size;
    if(frame.arguments)bytes+=frame.arguments->getDesc().size;
  }
  return bytes;
}
namespace {
bool unavailable(WorldRenderer& r,const char* reason) {
  r.batch->available=false;r.batch->prepared=false;
  std::printf("world_batch mode=legacy reason=%s required=%u\n",reason,r.batch->required?1u:0u);
  std::fflush(stdout);
  return !r.batch->required;
}
bool upload_buffer(WorldBatch& batch,std::uint64_t bytes,unsigned stride,rhi::BufferUsage usage,
    Slang::ComPtr<rhi::IBuffer>& buffer,void*& mapped) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.memoryType=rhi::MemoryType::Upload;
  desc.usage=usage;desc.defaultState=usage==rhi::BufferUsage::IndirectArgument?
      rhi::ResourceState::IndirectArgument:rhi::ResourceState::ShaderResource;
  return world_rhi_ok(batch.device->createBuffer(desc,nullptr,buffer.writeRef())) &&
      world_rhi_ok(batch.device->mapBuffer(buffer,rhi::CpuAccessMode::Write,&mapped)) && mapped;
}
bool pipelines(WorldBatch& batch) {
  const char* entries[]={"batch_vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(batch.device,"octaryn-client/Shaders/Voxel/WorldBatchRaster.slang",entries,2,program))return false;
  rhi::ColorTargetDesc targets[4]{};
  for(unsigned i=0;i<4;++i) targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=targets;desc.targetCount=4;
  desc.primitiveTopology=rhi::PrimitiveTopology::TriangleList;desc.depthStencil.format=rhi::Format::D32Float;
  desc.depthStencil.depthTestEnable=true;desc.depthStencil.depthWriteEnable=true;
  desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;desc.rasterizer.cullMode=rhi::CullMode::Back;
  if(!world_rhi_ok(batch.device->createRenderPipeline(desc,batch.opaque.writeRef())))return false;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  return world_rhi_ok(batch.device->createRenderPipeline(desc,batch.sprite.writeRef()));
}
bool handles(WorldColumnGpu& column) {
  if(column.batch_handles_ready)return true;
  rhi::DescriptorHandle faces{},patches{};
  if(!column.faces || !column.patches ||
      !world_rhi_ok(column.faces->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,rhi::Format::Undefined,rhi::kEntireBuffer,&faces)) ||
      !world_rhi_ok(column.patches->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,rhi::Format::Undefined,rhi::kEntireBuffer,&patches)) ||
      faces.type!=rhi::DescriptorHandleType::Buffer || patches.type!=rhi::DescriptorHandleType::Buffer)return false;
  column.batch_faces=faces.value;column.batch_patches=patches.value;column.batch_handles_ready=true;
  return true;
}
}
bool world_batch_initialize(WorldRenderer& r,bool descriptor_capacity_available) {
  r.batch=std::make_unique<WorldBatch>();auto& batch=*r.batch;batch.device=r.device;
  const char* mode=SDL_getenv("OCTARYN_CLIENT_WORLD_BATCH");
  if(mode && std::strcmp(mode,"auto") && std::strcmp(mode,"off") && std::strcmp(mode,"required")) {
    std::fprintf(stderr,"Invalid OCTARYN_CLIENT_WORLD_BATCH (auto, off, required)\n");return false;
  }
  batch.required=mode && !std::strcmp(mode,"required");
  if(mode && !std::strcmp(mode,"off"))return unavailable(r,"disabled");
  if(!descriptor_capacity_available)return unavailable(r,"descriptor_capacity_unavailable");
  if(!r.device->hasFeature(rhi::Feature::Bindless) || !r.device->hasFeature(rhi::Feature::MultiDrawIndirect) ||
      !r.device->hasFeature(rhi::Feature::DrawIndirectFirstInstance) || !r.device->hasFeature(rhi::Feature::ShaderDrawParameters))
    return unavailable(r,"device_features_unavailable");
  batch.max_draws=r.device->getInfo().limits.maxDrawIndirectCount;
  if(batch.max_draws<2)return unavailable(r,"indirect_draw_limit");
  if(!pipelines(batch))return unavailable(r,"batch_pipeline_unavailable");
  constexpr auto capacity=WorldBatchMaxColumns*2;
  for(auto& frame:batch.frames) {
    if(!upload_buffer(batch,capacity*sizeof(WorldBatchRecord),sizeof(WorldBatchRecord),rhi::BufferUsage::ShaderResource,frame.records,frame.mapped_records) ||
        !upload_buffer(batch,capacity*sizeof(WorldBatchArguments),sizeof(WorldBatchArguments),rhi::BufferUsage::IndirectArgument,frame.arguments,frame.mapped_arguments))
      return unavailable(r,"batch_upload_unavailable");
    frame.retained.reserve(capacity*2);
  }
  batch.cpu_records.reserve(capacity);batch.cpu_arguments.reserve(capacity);
  batch.available=true;
  std::printf("world_batch mode=bindless capacity=%u descriptors=%u max_draws=%u\n",capacity,WorldBatchDescriptorCapacity,batch.max_draws);
  std::fflush(stdout);return true;
}
bool world_batch_begin_frame(WorldRenderer& r,unsigned slot) {
  if(!r.batch)return true;
  if(slot>=r.batch->frames.size()) {r.status="invalid_batch_frame_slot";return false;}
  auto& batch=*r.batch;batch.active_slot=slot;batch.prepared=false;
  batch.frame().retained.clear();
  return true;
}
bool world_batch_prepare(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  if(!r.batch)return true;
  auto& batch=*r.batch;batch.prepared=false;batch.submitted_commands=0;batch.submitted_columns=0;
  // Only the selected slot is reused after its fence completes. Other slots
  // retain their mapped contents and all handle-addressed mesh owners.
  auto& frame=batch.frame();
  frame.retained.clear();batch.cpu_records.clear();batch.cpu_arguments.clear();batch.first={};batch.count={};
  if(!batch.available || !batch.enabled)return true;
  if(r.draw_list.visible.size()>WorldBatchMaxColumns)return unavailable(r,"column_capacity_exceeded");
  for(unsigned pass=0;pass<2;++pass) {
    batch.first[pass]=static_cast<std::uint32_t>(batch.cpu_records.size());
    for(std::size_t i=0;i<r.draw_list.visible.size();++i) {
      auto& column=*world_draw_item(r.draw_list,i,false).column;
      if(!column.pass_counts[pass])continue;
      if(!column.patch_counts[pass] || !handles(column))return unavailable(r,"column_handles_unavailable");
      const auto record=static_cast<std::uint32_t>(batch.cpu_records.size());
      const auto patch_base=pass==0?0u:column.patch_counts[0];
      batch.cpu_records.push_back({column.batch_faces,column.batch_patches,patch_base,0});
      batch.cpu_arguments.push_back({6,column.patch_counts[pass],0,record});
      frame.retained.push_back(column.faces);frame.retained.push_back(column.patches);
      // RHI cannot discover resources addressed through handles stored in a buffer.
      commands->setBufferState(column.faces,rhi::ResourceState::ShaderResource);
      commands->setBufferState(column.patches,rhi::ResourceState::ShaderResource);
      ++batch.count[pass];
    }
  }
  if(!batch.cpu_records.empty()) {
    std::memcpy(frame.mapped_records,batch.cpu_records.data(),batch.cpu_records.size()*sizeof(WorldBatchRecord));
    std::memcpy(frame.mapped_arguments,batch.cpu_arguments.data(),batch.cpu_arguments.size()*sizeof(WorldBatchArguments));
  }
  commands->setBufferState(frame.records,rhi::ResourceState::ShaderResource);
  commands->setBufferState(frame.arguments,rhi::ResourceState::IndirectArgument);
  batch.prepared=true;return true;
}
bool world_batch_draw(WorldRenderer& r,rhi::IRenderPassEncoder* render,std::size_t pass) {
  auto& batch=*r.batch;if(pass>=2 || !batch.prepared)return false;
  auto& frame=batch.frame();
  const auto count=batch.count[pass];if(!count)return true;
  auto* root=render->bindPipeline(pass==0?batch.opaque:batch.sprite);
  if(!root || !bind_world_atlas(r.atlas,root,1))return false;
  auto uniforms=r.draw_uniforms;uniforms[28]=0;
  if(!world_rhi_ok(rhi::ShaderCursor(root)["batchRecords"].setBinding(rhi::Binding(frame.records))) ||
      !world_rhi_ok(root->setData({0,0,0},uniforms.data(),sizeof(uniforms))))return false;
  for(std::uint32_t offset=0;offset<count;) {
    const auto draws=std::min(batch.max_draws,count-offset);
    render->drawIndirect(draws,{frame.arguments,(batch.first[pass]+offset)*sizeof(WorldBatchArguments)});
    ++batch.submitted_commands;batch.submitted_columns+=draws;offset+=draws;
  }
  return true;
}
}
