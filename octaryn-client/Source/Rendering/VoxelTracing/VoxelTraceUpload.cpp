#include "VoxelTraceUpload.h"
#include <slang-rhi/shader-cursor.h>

namespace octaryn::client::voxel_tracing {
namespace {
bool upload(rhi::ICommandEncoder* commands,rhi::IBuffer* buffer,std::uint64_t offset,std::uint64_t size,const void* data) {
  return !size || SLANG_SUCCEEDED(commands->uploadBufferData(buffer,offset,size,data));
}
bool resource(rhi::IShaderObject* root,const char* name,rhi::IBuffer* buffer) {
  const auto cursor=rhi::ShaderCursor(root)[name];return !cursor.isValid() || SLANG_SUCCEEDED(cursor.setBinding(rhi::Binding(buffer)));
}
template<class T> bool uniform(rhi::IShaderObject* root,const char* name,const T& value) {
  const auto cursor=rhi::ShaderCursor(root)[name];return !cursor.isValid() || SLANG_SUCCEEDED(cursor.setData(&value,sizeof(value)));
}
}
bool VoxelTraceUpload::initialize(rhi::IDevice* device,UploadConfig config) {
  if(device_ || !device)return false;
  layout_=trace_upload_layout(config);if(!layout_.capacity)return false;
  device_=device;config_=config;stats_.capacity=layout_.capacity;stats_.reserved_bytes=layout_.total_bytes;return true;
}
bool VoxelTraceUpload::allocate(Frame& frame) {
  if(frame.allocated)return true;
  const auto buffer=[&](Slang::ComPtr<rhi::IBuffer>& target,unsigned count,unsigned stride,const char* label) {
    if(target)return true;
    rhi::BufferDesc desc{};desc.size=std::uint64_t(count)*stride;desc.elementSize=stride;desc.label=label;
    desc.memoryType=rhi::MemoryType::DeviceLocal;
    desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
    desc.defaultState=rhi::ResourceState::ShaderResource;
    if(SLANG_FAILED(device_->createBuffer(desc,nullptr,target.writeRef())))return false;
    stats_.allocated_bytes+=desc.size;return true;
  };
  if(!buffer(frame.chunks,layout_.capacity,sizeof(ChunkHeader),"voxel_trace_chunks") ||
      !buffer(frame.leaves,layout_.capacity*LeafCount,8,"voxel_trace_leaves") ||
      !buffer(frame.macros,layout_.capacity*MacroCount,8,"voxel_trace_macros") ||
      !buffer(frame.materials,layout_.capacity*MaterialWordCount,4,"voxel_trace_materials") ||
      !buffer(frame.hash,layout_.hash_count,4,"voxel_trace_hash"))return false;
  frame.allocated=true;return true;
}
bool VoxelTraceUpload::prepare(rhi::ICommandEncoder* commands,const VoxelTraceWorld& world,unsigned slot,ChunkKey center) {
  if(!device_ || !commands || slot>=frames_.size())return false;
  auto& frame=frames_[slot];if(!allocate(frame))return false;
  auto plan=plan_trace_upload(world,frame.snapshot,layout_,config_,center);
  for(unsigned pool:plan.uploads) {
    const auto& chunk=*plan.snapshot.slots[pool];const auto materials=chunk.material_words();
    if(!upload(commands,frame.leaves,std::uint64_t(pool)*LeafCount*8,LeafCount*8,chunk.leaves.data()) ||
        !upload(commands,frame.macros,std::uint64_t(pool)*MacroCount*8,MacroCount*8,chunk.macros.data()) ||
        !upload(commands,frame.materials,std::uint64_t(pool)*MaterialWordCount*4,MaterialWordCount*4,materials.data()))return false;
  }
  if(plan.metadata_changed &&
      (!upload(commands,frame.chunks,0,plan.snapshot.headers.size()*sizeof(ChunkHeader),plan.snapshot.headers.data()) ||
       !upload(commands,frame.hash,0,plan.snapshot.hash.size()*4,plan.snapshot.hash.data())))return false;
  for(auto* buffer:{frame.chunks.get(),frame.leaves.get(),frame.macros.get(),frame.materials.get(),frame.hash.get()})
    commands->setBufferState(buffer,rhi::ResourceState::ShaderResource);
  stats_.uploaded_bytes=plan.upload_bytes;stats_.uploaded_chunks=unsigned(plan.uploads.size());
  stats_.resident=unsigned(plan.snapshot.headers.size());stats_.pending=plan.pending;stats_.capacity_unknown=plan.capacity_unknown;
  stats_.evicted=plan.evicted;stats_.invalidated=plan.invalidated;stats_.max_hash_probes=plan.max_hash_probes;
  stats_.journal_overflow=plan.journal_overflow;stats_.world_epoch=plan.snapshot.world_epoch;stats_.generation=plan.snapshot.generation;
  frame.snapshot=std::move(plan.snapshot);return true;
}
bool VoxelTraceUpload::bind(rhi::IShaderObject* root,unsigned slot) const {
  if(!root || slot>=frames_.size() || !frames_[slot].allocated)return false;
  const auto& frame=frames_[slot];
  const std::array<unsigned,4> counts{unsigned(frame.snapshot.headers.size()),layout_.capacity*LeafCount,
    layout_.capacity*MacroCount,layout_.capacity*MaterialWordCount};
  const unsigned mask=layout_.hash_count-1;
  const std::array<unsigned,2> generation{unsigned(frame.snapshot.generation),unsigned(frame.snapshot.generation>>32)};
  return resource(root,"voxelTraceChunks",frame.chunks) && resource(root,"voxelTraceLeaves",frame.leaves) &&
    resource(root,"voxelTraceMacros",frame.macros) && resource(root,"voxelTraceMaterials",frame.materials) &&
    resource(root,"voxelTraceHash",frame.hash) && uniform(root,"voxelTraceCounts",counts) &&
    uniform(root,"voxelTraceHashMask",mask) && uniform(root,"voxelTraceGeneration",generation);
}
}
