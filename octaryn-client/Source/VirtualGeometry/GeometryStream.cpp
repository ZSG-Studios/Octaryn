#include "GeometryStreamInternal.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
GeometryStream::GeometryStream():state_(std::make_unique<State>()) {}
GeometryStream::~GeometryStream()=default;
const GeometryAsset& GeometryStream::asset() const {return state_->asset;}
const PageResidency& GeometryStream::residency() const {return *state_->residency;}
std::vector<GpuPage> GeometryStream::page_table() const {return state_->residency?state_->residency->page_table():std::vector<GpuPage>{};}
rhi::IBuffer* GeometryStream::pool() const {return state_->pool;}
rhi::IBuffer* GeometryStream::clusters() const {return state_->clusters;}
const std::string& GeometryStream::error() const {return state_->error;}
bool GeometryStream::initialize(rhi::IDevice* device,const std::filesystem::path& path,const std::string& hash,
    const GeometryStreamConfig& config) {
  auto& s=*state_;
  if(s.device || !device || !config.slots || config.slots>16384 || !config.workers || config.workers>8 ||
      !config.feedback_capacity || !config.upload_pages || !std::isfinite(config.upload_ms) || config.upload_ms<=0)
    return s.fail("invalid geometry stream configuration");
  s.device=device;s.path=path;s.config=config;
  if(!read_geometry_cache(path,hash,s.asset,s.error,false)) {s.failed=true;return false;}
  s.pinned.resize(s.asset.pages.size());
  for(const auto root:s.asset.roots) {
    const auto& group=s.asset.groups[root];
    for(unsigned i=0;i<group.page_count;++i)s.pinned[s.asset.group_pages[group.first_page+i]]=true;
  }
  for(unsigned p=0;p<s.pinned.size();++p)if(s.pinned[p])s.roots.push_back({p,FLT_MAX});
  if(s.roots.size()>config.slots || s.roots.size()>config.feedback_capacity)
    return s.fail("geometry root pages exceed residency slots or feedback capacity");
  s.stats.pinned_pages=unsigned(s.roots.size());
  try {
    s.residency=std::make_unique<PageResidency>(unsigned(s.asset.pages.size()),config.slots,
        std::uint64_t(config.slots)*page_bytes,config.feedback_capacity);
  }catch(const std::exception& failure) {return s.fail(failure.what());}
  rhi::BufferDesc desc{};desc.size=std::uint64_t(config.slots)*page_bytes;desc.elementSize=0;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="virtual_geometry_page_pool";
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,s.pool.writeRef())))return s.fail("geometry page pool allocation failed");
  desc.size=s.asset.clusters.size()*sizeof(GeometryCluster);desc.elementSize=sizeof(GeometryCluster);
  desc.label="virtual_geometry_clusters";
  if(SLANG_FAILED(device->createBuffer(desc,s.asset.clusters.data(),s.clusters.writeRef())))return s.fail("geometry cluster table allocation failed");
  s.scheduler=octaryn_native_schedule_runtime_create(int(config.workers),int(config.workers));
  if(!s.scheduler)return s.fail("geometry decode scheduler initialization failed");
  for(unsigned i=0;i<config.workers;++i)s.jobs.push_back(std::make_unique<State::Job>());
  s.residency->feedback(s.roots);s.error.clear();return true;
}
bool GeometryStream::pump(rhi::ICommandEncoder* commands,std::span<const PageRequest> requests,FenceValues completed) {
  auto& s=*state_;
  if(s.failed)return false;
  if(!s.residency || !commands || s.recorded)return s.fail("geometry pump missing initialization or prior submission");
  std::uint64_t actual{};
  if(s.fence && (SLANG_FAILED(s.fence->getCurrentValue(&actual)) || actual==UINT64_MAX))return s.fail("geometry submission fence failed");
  completed.upload=actual;completed.raster=actual;s.residency->complete(completed);
  s.extra_completed.ray=std::max(s.extra_completed.ray,completed.ray);
  s.extra_completed.pose=std::max(s.extra_completed.pose,completed.pose);
  if(!s.poll_jobs())return false;
  s.stats.uploaded_this_pump=0;s.recorded=true;
  const auto start=std::chrono::steady_clock::now();
  for(auto& pointer:s.jobs) {
    auto& job=*pointer;if(!job.ready)continue;
    if(s.stats.uploaded_this_pump>=s.config.upload_pages ||
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>=s.config.upload_ms)break;
    if(!s.residency->valid(job.handle))return s.fail("geometry upload reservation became stale");
    if(SLANG_FAILED(commands->uploadBufferData(s.pool,std::uint64_t(job.handle.slot)*page_bytes,page_bytes,job.decoded.data())))
      return s.fail("geometry page upload recording failed");
    s.recorded_uploads.push_back(job.handle);job.ready=false;job.decoded.clear();
    ++s.stats.uploaded_this_pump;++s.stats.uploaded_pages;s.stats.uploaded_bytes+=page_bytes;
  }
  if(!s.recorded_uploads.empty())commands->setBufferState(s.pool,rhi::ResourceState::ShaderResource);
  s.residency->feedback(requests);
  if(!s.start_jobs())return false;
  s.recorded_references.clear();
  for(unsigned p=0;p<s.asset.pages.size();++p)if(s.residency->resident(p))s.recorded_references.push_back(s.residency->handle(p));
  return true;
}
bool GeometryStream::submitted(rhi::IFence* fence,std::uint64_t value,FenceValues consumers) {
  auto& s=*state_;
  if(!s.recorded || !fence || !value || value<=s.signal || (s.fence && s.fence.get()!=fence))
    return s.fail("geometry submission requires one increasing fence timeline");
  s.fence=fence;s.signal=value;
  for(auto handle:s.recorded_uploads)if(!s.residency->begin_upload(handle,value))return s.fail("geometry upload publication failed");
  consumers.upload=0;consumers.raster=value;
  s.extra_required.ray=std::max(s.extra_required.ray,consumers.ray);
  s.extra_required.pose=std::max(s.extra_required.pose,consumers.pose);
  for(auto handle:s.recorded_references)if(!s.residency->reference(handle,consumers))return s.fail("geometry frame reference stale");
  s.recorded_uploads.clear();s.recorded_references.clear();s.recorded=false;return true;
}
void GeometryStream::touch_used(std::span<const std::uint32_t> pages) {
  auto& s=*state_;if(!s.residency)return;
  for(auto page:pages)s.residency->touch(page);
}
bool GeometryStream::roots_ready() const {
  const auto& s=*state_;return s.residency && std::all_of(s.roots.begin(),s.roots.end(),[&](auto root){return s.residency->resident(root.page);});
}
bool GeometryStream::gpu_idle() const {
  const auto& s=*state_;
  if(s.recorded || s.extra_completed.ray<s.extra_required.ray || s.extra_completed.pose<s.extra_required.pose)return false;
  if(!s.fence)return true;std::uint64_t value{};
  return SLANG_SUCCEEDED(s.fence->getCurrentValue(&value)) && value!=UINT64_MAX && value>=s.signal;
}
GeometryStreamStats GeometryStream::stats() const {
  const auto& s=*state_;auto output=s.stats;if(s.residency)output.residency=s.residency->stats();
  for(const auto& job:s.jobs) {output.loading_pages+=job->task!=nullptr;output.ready_pages+=job->ready;}
  return output;
}
}
