#include "WorldRendererInternal.h"
#include "ShadowBatch.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
constexpr unsigned Capacity=WorldBatchMaxColumns*6;
bool upload_buffer(ShadowBatch& batch,std::uint64_t bytes,unsigned stride,rhi::BufferUsage usage,
    Slang::ComPtr<rhi::IBuffer>& buffer,void*& mapped) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;desc.memoryType=rhi::MemoryType::Upload;
  desc.usage=usage;desc.defaultState=usage==rhi::BufferUsage::IndirectArgument?
      rhi::ResourceState::IndirectArgument:rhi::ResourceState::ShaderResource;
  return world_rhi_ok(batch.device->createBuffer(desc,nullptr,buffer.writeRef())) &&
      world_rhi_ok(batch.device->mapBuffer(buffer,rhi::CpuAccessMode::Write,&mapped)) && mapped;
}
bool handle(rhi::IBuffer* buffer,std::uint64_t& value) {
  rhi::DescriptorHandle result{};
  if(!buffer || !world_rhi_ok(buffer->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,
      rhi::Format::Undefined,rhi::kEntireBuffer,&result)) || result.type!=rhi::DescriptorHandleType::Buffer)return false;
  value=result.value;return true;
}
}
ShadowBatch::~ShadowBatch() {
  for(auto& frame:frames) {
    if(device && frame.mapped_records)device->unmapBuffer(frame.records);
    if(device && frame.mapped_arguments)device->unmapBuffer(frame.arguments);
  }
}
std::uint64_t ShadowBatch::gpu_bytes() const {
  std::uint64_t bytes{};
  for(const auto& frame:frames)for(auto* buffer:{frame.records.get(),frame.arguments.get()})
    if(buffer)bytes+=buffer->getDesc().size;
  return bytes;
}
bool shadow_batch_initialize(WorldRenderer& r) {
  auto& b=r.shadow_fallback.batch;
  const auto& c=r.capabilities;
  if(!c.bindless || !c.multi_draw_indirect || !c.draw_indirect_first_instance || !c.shader_draw_parameters ||
      c.buffer_descriptors<WorldBatchDescriptorCapacity || !c.max_indirect_draws)return true;
  b.device=r.device;b.max_draws=c.max_indirect_draws;
  const char* entries[]={"batch_vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(r.device,"octaryn-client/Shaders/Shadows/ClipmapRaster.slang",entries,2,program))return false;
  rhi::RenderPipelineDesc p{};p.program=program;p.primitiveTopology=rhi::PrimitiveTopology::TriangleList;
  p.depthStencil.format=rhi::Format::D32Float;p.depthStencil.depthTestEnable=true;p.depthStencil.depthWriteEnable=true;
  p.depthStencil.depthFunc=rhi::ComparisonFunc::Less;p.rasterizer.cullMode=rhi::CullMode::None;
  if(!world_rhi_ok(r.device->createRenderPipeline(p,b.raster.writeRef())))return false;
  for(auto& frame:b.frames) {
    if(!upload_buffer(b,Capacity*sizeof(ShadowBatchRecord),sizeof(ShadowBatchRecord),rhi::BufferUsage::ShaderResource,
        frame.records,frame.mapped_records) ||
       !upload_buffer(b,Capacity*sizeof(WorldBatchArguments),sizeof(WorldBatchArguments),rhi::BufferUsage::IndirectArgument,
        frame.arguments,frame.mapped_arguments))return false;
    frame.retained.reserve(WorldBatchMaxColumns*2);
  }
  b.columns.reserve(WorldBatchMaxColumns);b.records.reserve(Capacity);b.arguments.reserve(Capacity);b.available=true;
  std::printf("world_shadow_batch mode=bindless capacity=%u max_draws=%u gpu_bytes=%llu\n",Capacity,b.max_draws,
      static_cast<unsigned long long>(b.gpu_bytes()));
  return true;
}
bool shadow_batch_begin_frame(ShadowBatch& b,unsigned slot) {
  if(slot>=b.frames.size())return false;
  b.active_slot=slot;b.prepared=false;b.submitted_commands=b.submitted_draws=0;
  b.frames[slot].retained.clear();return true;
}
bool shadow_column_visible(std::pair<std::int32_t,std::int32_t> coordinate,const WorldColumnGpu& column,
    const std::array<float,4>& center,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward) {
  const float p[3]={float(coordinate.first)*32+16-center[0],float(column.min_y)+float(column.height)*.5f-center[1],
      float(coordinate.second)*32+16-center[2]};
  const float radius=std::sqrt(512.f+float(column.height)*float(column.height)*.25f);
  const auto distance=[&](const auto& a) {return std::abs(p[0]*a[0]+p[1]*a[1]+p[2]*a[2]);};
  return distance(right)<=center[3]+radius && distance(up)<=center[3]+radius && distance(forward)<=1024+radius;
}
bool shadow_batch_prepare(WorldRenderer& r,rhi::ICommandEncoder* commands,
    const std::array<std::array<float,4>,3>& centers,const std::array<float,4>& right,
    const std::array<float,4>& up,const std::array<float,4>& forward) {
  auto& b=r.shadow_fallback.batch;
  if(!shadow_batch_begin_frame(b,r.active_frame))return false;
  b.records.clear();b.arguments.clear();b.columns.clear();b.first={};b.count={};
  if(!b.available || !b.enabled)return true;
  if(r.columns.size()>WorldBatchMaxColumns)return false;
  auto& frame=b.frames[b.active_slot];
  for(const auto& [coordinate,column]:r.columns) {
    if(!column.pass_counts[0] && !column.pass_counts[1] && !column.pass_counts[4])continue;
    ShadowBatchColumn entry{coordinate,&column};
    if(!handle(column.faces,entry.faces) || !handle(column.fluids,entry.fluids))return false;
    b.columns.push_back(entry);frame.retained.push_back(column.faces);frame.retained.push_back(column.fluids);
  }
  for(unsigned level=0;level<3;++level) {
    b.first[level]=static_cast<unsigned>(b.records.size());
    for(const auto& entry:b.columns) {
      const auto& column=*entry.column;
      if(!shadow_column_visible(entry.coordinate,column,centers[level],right,up,forward))continue;
      const auto opaque=column.pass_counts[0]+column.pass_counts[1],fluid_base=opaque+column.pass_counts[2];
      const auto add=[&](unsigned first,unsigned count) {
        if(!count)return;
        b.arguments.push_back({6,count,0,static_cast<unsigned>(b.records.size())});
        b.records.push_back({entry.faces,entry.fluids,first,fluid_base});++b.count[level];
      };
      add(0,opaque);add(fluid_base+column.pass_counts[3],column.pass_counts[4]);
    }
  }
  // Meshes are immutable ShaderResource buffers; these fenced owners keep all
  // handle-addressed geometry alive without per-column state commands.
  b.columns.clear();
  if(!b.records.empty()) {
    std::memcpy(frame.mapped_records,b.records.data(),b.records.size()*sizeof(ShadowBatchRecord));
    std::memcpy(frame.mapped_arguments,b.arguments.data(),b.arguments.size()*sizeof(WorldBatchArguments));
  }
  commands->setBufferState(frame.records,rhi::ResourceState::ShaderResource);
  commands->setBufferState(frame.arguments,rhi::ResourceState::IndirectArgument);
  b.prepared=true;return true;
}
bool shadow_batch_draw(WorldRenderer& r,rhi::IRenderPassEncoder* pass,rhi::IShaderObject* root,unsigned level) {
  auto& b=r.shadow_fallback.batch;if(!b.prepared || level>=3 || !root)return false;
  const auto& frame=b.frames[b.active_slot];
  if(!world_rhi_ok(rhi::ShaderCursor(root)["shadowRecords"].setBinding(rhi::Binding(frame.records))))return false;
  for(unsigned offset=0;offset<b.count[level];) {
    const auto count=std::min(b.max_draws,b.count[level]-offset);
    pass->drawIndirect(count,{frame.arguments,(b.first[level]+offset)*sizeof(WorldBatchArguments)});
    offset+=count;++b.submitted_commands;b.submitted_draws+=count;
  }
  return true;
}
}
