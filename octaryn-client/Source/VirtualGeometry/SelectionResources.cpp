#include "SelectionResourcesInternal.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <algorithm>

namespace octaryn::client::rendering::virtual_geometry {
SelectionResources::SelectionResources():state_(std::make_unique<State>()) {}
SelectionResources::~SelectionResources()=default;
std::uint64_t SelectionResources::required_bytes(const SelectionResourcesConfig& c) {
  return scene_selection_bytes(c);
}
bool SelectionResources::State::buffer(Slang::ComPtr<rhi::IBuffer>& output,std::uint64_t bytes,unsigned stride,bool readback) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;
  desc.usage=readback?rhi::BufferUsage::CopyDestination:
      rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|
      rhi::BufferUsage::CopySource|rhi::BufferUsage::IndirectArgument;
  desc.defaultState=readback?rhi::ResourceState::CopyDestination:rhi::ResourceState::ShaderResource;
  if(readback)desc.memoryType=rhi::MemoryType::ReadBack;
  desc.label=readback?"scene_selection_feedback":"scene_selection_scratch";
  return SLANG_SUCCEEDED(device->createBuffer(desc,nullptr,output.writeRef())) || fail("shared selection allocation failed");
}
bool SelectionResources::initialize(rhi::IDevice* device,std::shared_ptr<SceneMemoryLedger> ledger,
    const char* shader,const SelectionResourcesConfig& c) {
  auto& s=*state_;
  if(s.device || !device || !ledger || !shader || !c.groups || c.groups>1048576 ||
      !c.clusters || c.clusters>1048576 || !c.pages || c.pages>65536 ||
      !c.page_references || c.page_references>4194304 || !c.parents || c.parents>4194304 ||
      !c.instances || c.instances>65536 || !c.feedback_capacity || c.feedback_capacity>65536 ||
      !c.frame_count || c.frame_count>8 || !c.tickets_per_frame || c.tickets_per_frame>65536 ||
      c.readback_bytes<24+std::uint64_t(c.feedback_capacity)*8+std::uint64_t(c.pages)*4 ||
      c.readback_bytes>64ull*1024*1024)return s.fail("invalid shared selection configuration");
  s.device=device;s.config=c;s.allocation=ledger->reserve(required_bytes(c),SceneMemoryDomain::Selection);
  if(!s.allocation)return s.fail("shared selection exceeds scene memory budget");
  s.feedback_memory->limit=c.readback_bytes*c.frame_count;
  s.owners.reserve(c.tickets_per_frame);
  const char* names[]={"reset_main","select_main","compact_main","finish_main","feedback_main"};
  for(unsigned i=0;i<s.pipelines.size();++i)
    if(!create_rhi_compute_pipeline(device,shader,names[i],s.pipelines[i]))return s.fail("shared selection shader compilation failed");
  s.banks.resize(c.frame_count);
  for(auto& bank:s.banks) {
    auto& f=bank.buffers;bank.tickets.reserve(c.tickets_per_frame);
    if(!s.buffer(f.groups,std::uint64_t(c.groups)*sizeof(SelectionGroup),sizeof(SelectionGroup)) ||
        !s.buffer(f.clusters,std::uint64_t(c.clusters)*sizeof(SelectionCluster),sizeof(SelectionCluster)) ||
        !s.buffer(f.group_pages,std::uint64_t(c.page_references)*4,4) || !s.buffer(f.parents,std::uint64_t(c.parents)*4,4) ||
        !s.buffer(f.pages,std::uint64_t(c.pages)*16,16) || !s.buffer(f.active,std::uint64_t(c.groups)*4,4) ||
        !s.buffer(f.requested,std::uint64_t(c.pages)*4,4) || !s.buffer(f.priorities,std::uint64_t(c.pages)*4,4) ||
        !s.buffer(f.used,std::uint64_t(c.pages)*4,4) || !s.buffer(f.feedback,std::uint64_t(c.feedback_capacity)*8,8) ||
        !s.buffer(f.selected,std::uint64_t(c.clusters)*16,16) || !s.buffer(f.counters,24,4) || !s.buffer(f.dispatch,24,4) ||
        !s.buffer(f.instances,std::uint64_t(c.instances)*sizeof(InstanceSelectionView),sizeof(InstanceSelectionView)) ||
        !s.buffer(f.readback,c.readback_bytes,4,true))return false;
  }
  s.error.clear();return true;
}
std::uint64_t SelectionResources::feedback_bytes(std::uint32_t pages,std::uint32_t capacity) {
  return scene_selection_feedback_bytes(pages,capacity);
}
std::uint32_t SelectionResources::feedback_capacity(std::uint32_t pages) const {
  return std::min(pages,state_->config.feedback_capacity);
}
std::uint64_t SelectionResources::register_owner(std::uint32_t pages,std::uint32_t capacity) {
  auto& s=*state_;const auto bytes=feedback_bytes(pages,capacity);
  if(!pages || pages>s.config.pages || !capacity || capacity>s.config.feedback_capacity ||
      s.next_owner==UINT64_MAX || s.owners.size()==s.config.tickets_per_frame ||
      bytes>s.config.readback_bytes-s.reserved_feedback) {
    s.fail("shared selection complete feedback admission exceeded");return 0;
  }
  const auto id=s.next_owner+1;s.owners.emplace(id,bytes);s.next_owner=id;s.reserved_feedback+=bytes;return id;
}
void SelectionResources::unregister_owner(std::uint64_t owner) {
  auto& s=*state_;const auto found=s.owners.find(owner);if(found==s.owners.end())return;
  s.reserved_feedback-=found->second;s.owners.erase(found);
}
bool SelectionResources::supports(const SelectionTopology& t,std::uint32_t capacity) const {
  const auto& c=state_->config;
  return !t.groups.empty() && !t.clusters.empty() && !t.pages.empty() &&
      t.groups.size()<=c.groups && t.clusters.size()<=c.clusters && t.pages.size()<=c.page_references &&
      t.parents.size()<=c.parents && *std::max_element(t.pages.begin(),t.pages.end())<c.pages &&
      capacity && capacity<=c.feedback_capacity;
}
rhi::IDevice* SelectionResources::device() const {return state_->device;}
std::uint64_t SelectionResources::gpu_bytes() const {return state_->allocation?state_->allocation->bytes():0;}
const std::string& SelectionResources::error() const {return state_->error;}
}
