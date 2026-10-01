#include "SceneGeometryPoolInternal.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numeric>

namespace octaryn::client::rendering::virtual_geometry {
SceneGeometryPool::SceneGeometryPool():state_(std::make_unique<State>()) {}
SceneGeometryPool::~SceneGeometryPool()=default;
SceneGeometryPool::State::Asset* SceneGeometryPool::State::find(SceneGeometryHandle id) {
  return id && id.slot<assets.size() && assets[id.slot].generation==id.generation &&
      !assets[id.slot].hash.empty()?&assets[id.slot]:nullptr;
}
const SceneGeometryPool::State::Asset* SceneGeometryPool::State::find(SceneGeometryHandle id) const {
  return const_cast<State*>(this)->find(id);
}
bool SceneGeometryPool::initialize(rhi::IDevice* device,std::shared_ptr<SceneMemoryLedger> ledger,
    const SceneGeometryPoolConfig& config) {
  auto& s=*state_;
  if(s.device || !device || !ledger || !config.slots || config.slots>8192 ||
      !config.root_slots || config.root_slots>=config.slots ||
      !config.maximum_pages || config.maximum_pages>1048576 || config.maximum_pages<config.slots ||
      !config.maximum_assets || config.maximum_assets>65536 || !config.feedback_capacity ||
      config.feedback_capacity>65536 || !config.workers || config.workers>8 || !config.upload_pages ||
      !std::isfinite(config.upload_ms) || config.upload_ms<=0)return s.fail("invalid shared geometry pool configuration");
  s.config=config;s.device=device;
  const auto bytes=std::uint64_t(config.slots)*page_bytes;
  s.allocation=ledger->reserve(bytes,SceneMemoryDomain::Pages);
  if(!s.allocation)return s.fail("shared geometry pool exceeds scene memory budget");
  rhi::BufferDesc desc{};desc.size=bytes;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="scene_geometry_page_pool";
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,s.pool.writeRef()))) {
    s.allocation.reset();return s.fail("shared geometry pool allocation failed");
  }
  const auto fine_slots=config.slots-config.root_slots;
  s.residency=std::make_unique<PageResidency>(config.maximum_pages,fine_slots,std::uint64_t(fine_slots)*page_bytes,config.feedback_capacity);
  s.root_pages=std::make_unique<SceneRootPages>(config.root_slots);
  s.assets.resize(config.maximum_assets);s.owners.resize(config.maximum_pages);
  s.free_pages.resize(config.maximum_pages);std::iota(s.free_pages.rbegin(),s.free_pages.rend(),0u);
  s.hashes.reserve(config.maximum_assets);s.reference_generations.resize(config.slots);
  s.uploads.reserve(config.upload_pages);s.references.reserve(fine_slots);s.consumers.reserve(config.maximum_assets);
  s.root_uploads.reserve(config.upload_pages);s.root_references.reserve(config.maximum_pages);
  s.scheduler=config.scheduler;
  if(!s.scheduler)s.scheduler=std::shared_ptr<void>(
      octaryn_native_schedule_runtime_create(int(config.workers),int(config.workers)),octaryn_native_schedule_runtime_destroy);
  if(!s.scheduler)return s.fail("shared geometry scheduler creation failed");
  for(unsigned i=0;i<config.workers;++i)s.jobs.push_back(std::make_unique<State::Job>());
  s.error.clear();return true;
}
bool SceneGeometryPool::register_asset(const std::filesystem::path& path,const GeometryAsset& source,SceneGeometryHandle& output) {
  auto& s=*state_;output={};s.error.clear();s.admission_rejected=false;
  if(s.failed || !s.residency || source.source_hash.empty() || source.pages.empty())return s.fail("invalid shared geometry registration");
  if(!s.encoder && !s.poll())return false;
  if(const auto found=s.hashes.find(source.source_hash);found!=s.hashes.end()) {
    auto* asset=s.find(found->second);
    if(!asset || asset->retiring || asset->users==UINT32_MAX)return s.reject("shared geometry asset still retiring");
    if(asset->descriptors.size()!=source.pages.size())return s.fail("shared geometry content identity collision");
    for(unsigned i=0;i<source.pages.size();++i)
      if(asset->descriptors[i].checksum!=source.pages[i].checksum)return s.fail("shared geometry page identity collision");
    ++asset->users;output=found->second;return true;
  }
  if(source.pages.size()>s.free_pages.size())return s.reject("shared geometry metadata page capacity exceeded");
  unsigned slot=invalid_id;
  for(unsigned i=0;i<s.assets.size();++i)if(s.assets[i].hash.empty() && s.assets[i].generation!=UINT32_MAX) {slot=i;break;}
  if(slot==invalid_id)return s.reject("shared geometry asset capacity exceeded");
  std::vector<bool> pinned(source.pages.size());
  for(const auto root:source.roots) {
    if(root>=source.groups.size())return s.fail("shared geometry root is outside metadata");
    const auto& group=source.groups[root];
    for(unsigned i=0;i<group.page_count;++i)pinned.at(source.group_pages.at(group.first_page+i))=true;
  }
  const auto roots=std::count(pinned.begin(),pinned.end(),true);
  const auto sizes=geometry_page_payload_bytes(source);
  struct Rollback {
    SceneRootPages& pool;std::vector<SceneRootSpan> spans;bool committed{};
    ~Rollback() {if(!committed)for(auto span:spans)if(span)pool.release(span,0,0);}
  } rollback{*s.root_pages,std::vector<SceneRootSpan>(source.pages.size())};
  for(unsigned page=0;page<source.pages.size();++page)if(pinned[page]) {
    rollback.spans[page]=s.root_pages->reserve(sizes[page]);
    if(!rollback.spans[page])return s.reject("shared geometry packed roots exceed slab capacity");
  }
  const SceneGeometryHandle id{slot,s.assets[slot].generation+1};
  State::Asset next;next.generation=id.generation;next.users=1;next.hash=source.source_hash;
  next.path=path;next.descriptors=source.pages;next.pages.reserve(source.pages.size());
  for(unsigned local=0;local<source.pages.size();++local) {
    const auto global=s.free_pages[s.free_pages.size()-1-local];next.pages.push_back(global);
    if(pinned[local])next.roots.push_back(global);
  }
  std::vector<PageRequest> requests;requests.reserve(next.roots.size());
  for(auto page:next.roots)requests.push_back({page,FLT_MAX});
  s.hashes.emplace(next.hash,id);s.assets[slot]=std::move(next);
  auto& asset=s.assets[slot];s.free_pages.resize(s.free_pages.size()-asset.pages.size());s.pinned+=unsigned(roots);
  for(unsigned local=0;local<asset.pages.size();++local)s.owners[asset.pages[local]]={id,local,rollback.spans[local]};
  rollback.committed=true;s.residency->feedback(requests);output=id;return true;
}
void SceneGeometryPool::release(SceneGeometryHandle id) {
  auto& s=*state_;auto* asset=s.find(id);if(!asset || !asset->users)return;
  if(--asset->users)return;
  asset->retiring=true;s.pinned-=unsigned(asset->roots.size());
  if(!s.encoder)s.collect();
}
void SceneGeometryPool::touch_used(SceneGeometryHandle id,std::span<const std::uint32_t> pages) {
  auto& s=*state_;const auto* asset=s.find(id);if(!asset || asset->retiring)return;
  for(auto page:pages)if(page<asset->pages.size())s.residency->touch(asset->pages[page]);
}
std::vector<GpuPage> SceneGeometryPool::page_table(SceneGeometryHandle id) const {
  const auto& s=*state_;const auto* asset=s.find(id);if(!asset || asset->retiring)return {};
  std::vector<GpuPage> result(asset->pages.size());
  for(unsigned i=0;i<asset->pages.size();++i) {
    const auto page=asset->pages[i];const auto& owner=s.owners[page];
    if(owner.root) {
      result[i]={owner.root.slot,owner.root.generation,unsigned(owner.upload_signal && owner.upload_signal<=s.completed),owner.root.offset};
    } else {
      const auto h=s.residency->handle(page);
      if(s.residency->valid(h))result[i]={h.slot+s.config.root_slots,h.generation,unsigned(s.residency->resident(page)),0};
    }
  }
  return result;
}
bool SceneGeometryPool::roots_ready(SceneGeometryHandle id) const {
  const auto& s=*state_;const auto* asset=s.find(id);
  return asset && !asset->retiring && std::all_of(asset->roots.begin(),asset->roots.end(),[&](auto p){
    const auto& owner=s.owners[p];return owner.upload_signal && owner.upload_signal<=s.completed;
  });
}
bool SceneGeometryPool::idle(SceneGeometryHandle id) const {
  const auto& s=*state_;const auto* asset=s.find(id);if(!asset)return true;
  if(s.encoder && asset->recorded_epoch==s.epoch)return false;
  std::uint64_t value{};
  return !s.fence || (SLANG_SUCCEEDED(s.fence->getCurrentValue(&value)) && value!=UINT64_MAX && value>=asset->last_signal);
}
rhi::IBuffer* SceneGeometryPool::buffer() const {return state_->pool;}
std::uint64_t SceneGeometryPool::gpu_bytes() const {return state_->pool?state_->pool->getDesc().size:0;}
const std::string& SceneGeometryPool::error() const {return state_->error;}
bool SceneGeometryPool::admission_rejected() const {return state_->admission_rejected;}
SceneGeometryPoolStats SceneGeometryPool::stats() const {
  const auto& s=*state_;auto result=s.counters;if(s.residency)result.residency=s.residency->stats();
  result.residency.budget=gpu_bytes();result.packed_root_bytes=s.root_pages?s.root_pages->bytes():0;
  result.residency.bytes+=result.packed_root_bytes;
  result.pinned_pages=s.pinned;result.registered_pages=unsigned(s.owners.size()-s.free_pages.size());
  for(const auto& asset:s.assets)if(!asset.hash.empty()) {
    ++result.assets;
    for(auto page:asset.roots) {
      const auto& owner=s.owners[page];
      const bool ready=owner.upload_signal && owner.upload_signal<=s.completed;
      result.residency.resident+=ready;result.residency.pending+=!ready;
    }
  }
  for(const auto& job:s.jobs) {result.loading_pages+=job->task!=nullptr;result.ready_pages+=job->ready;}
  return result;
}
}
