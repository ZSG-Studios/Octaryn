#include "RayGeometryInternal.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace octaryn::client::rendering::virtual_geometry {
namespace ray_geometry {
Slang::ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,std::uint64_t size,std::uint32_t stride,rhi::BufferUsage usage,const void* initial) {
  check(size>0&&size<=device->getInfo().limits.maxBufferSize,"ray geometry buffer limit exceeded");
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;
  desc.usage=usage|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  desc.defaultState=rhi::is_set(usage,rhi::BufferUsage::UnorderedAccess)?rhi::ResourceState::UnorderedAccess:rhi::ResourceState::AccelerationStructureBuildInput;
  Slang::ComPtr<rhi::IBuffer> result;checked(device->createBuffer(desc,initial,result.writeRef()),"ray buffer allocation failed");return result;
}
Slang::ComPtr<rhi::IAccelerationStructure> acceleration(rhi::IDevice* device,rhi::AccelerationStructureKind kind,std::uint64_t size) {
  check(size>0,"empty acceleration structure size");rhi::AccelerationStructureDesc desc{};desc.kind=kind;desc.size=size;desc.label="virtual_geometry_ray_scene";
  Slang::ComPtr<rhi::IAccelerationStructure> result;checked(device->createAccelerationStructure(desc,result.writeRef()),"ray acceleration allocation failed");return result;
}
bool completed(rhi::IFence* fence,std::uint64_t expected) {
  check(fence&&expected,"invalid ray completion fence");std::uint64_t value{};checked(fence->getCurrentValue(&value),"ray fence query failed");
  check(value!=UINT64_MAX,"ray fence reports device loss");return value>=expected;
}
std::vector<std::uint32_t> spatial_order(const GeometryAsset& asset,std::vector<std::uint32_t> clusters) {
  float minimum[3]{},maximum[3]{};bool first=true;
  for(auto id:clusters) {const auto& b=asset.clusters[id].bounds;for(int c=0;c<3;++c) {minimum[c]=first?b.center[c]:std::min(minimum[c],b.center[c]);maximum[c]=first?b.center[c]:std::max(maximum[c],b.center[c]);}first=false;}
  const auto morton=[&](std::uint32_t id) {
    std::uint32_t code=0;for(int c=0;c<3;++c) {
      const float value=(asset.clusters[id].bounds.center[c]-minimum[c])/std::max(1e-6f,maximum[c]-minimum[c]);
      const auto quantized=static_cast<std::uint32_t>(std::clamp(value,0.f,1.f)*1023);
      for(int bit=0;bit<10;++bit)code|=((quantized>>bit)&1)<<(bit*3+c);
    }return code;
  };
  std::vector<std::pair<std::uint32_t,std::uint32_t>> order;order.reserve(clusters.size());
  for(const auto id:clusters)order.emplace_back(morton(id),id);
  std::sort(order.begin(),order.end());
  for(std::size_t i=0;i<order.size();++i)clusters[i]=order[i].second;
  return clusters;
}
}
using namespace ray_geometry;
RayGeometry::RayGeometry():state_(std::make_unique<State>()) {}
RayGeometry::~RayGeometry() {
  if(state_->config.scheduler)state_->config.scheduler->release(state_.get());
}
std::uint64_t RayGeometry::State::live_bytes() const {
  std::set<const RaySnapshot*> unique;std::uint64_t bytes=0;
  if(current) {unique.insert(current.get());bytes=current->bytes;}
  for(const auto& weak:retired)if(const auto scene=weak.lock();scene && unique.insert(scene.get()).second)bytes+=scene->bytes;
  for(const auto& use:uses)if(unique.insert(use.scene.get()).second)bytes+=use.scene->bytes;
  return bytes;
}
bool RayGeometry::initialize(rhi::IDevice* device,const GeometryAsset& asset,const char* path,const RayGeometryConfig& config) {
  try {
    check(device&&path&&device->hasFeature(rhi::Feature::AccelerationStructure),"ray geometry acceleration structures unavailable");
    check(!state_->pending&&!state_->current&&state_->uses.empty(),"ray geometry already initialized");
    check(config.clusters_per_blas>=16&&config.clusters_per_blas<=1024&&config.maximum_clusters>0&&config.maximum_clusters<=65535,"invalid spatial ray batch limits");
    check(std::isfinite(config.error_pixels)&&config.error_pixels>=0&&config.maximum_build_bytes>0&&config.maximum_resident_bytes>0,"invalid ray geometry budget");
    std::string error;if(!validate_geometry(asset,error))throw std::runtime_error(error);
    auto next=std::make_unique<State>();next->device=device;next->asset=asset;next->asset.payloads.clear();next->config=config;
    if(!build_selection_topology(asset,next->topology,error))throw std::runtime_error(error);
    if(config.scheduler) {
      check(!config.build_local_tlas,"scene ray scheduler requires world TLAS ownership");
      check(config.scheduler->initialize(device,path),config.scheduler->error().c_str());
      next->expand=config.scheduler->expansion();
    } else check(create_rhi_compute_pipeline(device,path,"expandRayGeometry",next->expand),"ray expansion pipeline failed");
    state_=std::move(next);return true;
  } catch(const std::exception& e) {state_->error=e.what();return false;}
}
bool RayGeometry::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;if(!s.pending||s.pending->fence||!fence||!value) {s.error="invalid ray build submission";return false;}
  if(s.config.scheduler && !s.config.scheduler->submitted(&s,fence,value)) {s.error=s.config.scheduler->error();return false;}
  s.pending->fence=fence;s.pending->value=value;s.error.clear();return true;
}
void RayGeometry::cancel_unsubmitted() {
  auto& s=*state_;
  if(s.pending && s.pending->fence)return;
  s.pending.reset();
  if(s.config.scheduler && !s.budget.deferred)s.config.scheduler->release(&s);
}
bool RayGeometry::poll() {
  auto& s=*state_;try {
    s.error.clear();std::erase_if(s.uses,[](const auto& use) {return completed(use.fence,use.value);});
    std::erase_if(s.retired,[](const auto& weak) {return weak.expired();});
    if(!s.pending||!s.pending->fence||!completed(s.pending->fence,s.pending->value))return false;
    auto& pending=*s.pending;check(pending.recorded,"incomplete ray build was retired without publishing");
    if(pending.validation) {
      std::uint32_t invalid{};checked(s.device->readBuffer(pending.validation,0,sizeof(invalid),&invalid),"ray build validation readback failed");
      check(invalid==0,"ray source generation or local index validation failed");
      for(auto* buffer:{pending.copies.get(),pending.pages.get(),pending.validation.get()})
        if(buffer)pending.build_bytes-=buffer->getDesc().size;
      pending.copies.setNull();pending.pages.setNull();pending.validation.setNull();pending.source_pages.clear();
    }
    if(pending.phase!=State::Build::Phase::Complete) {
      pending.build_bytes-=pending.compact_source_bytes;pending.compact_source_bytes=0;
      pending.compact_sources.clear();pending.fence.setNull();pending.value=0;pending.ready=true;return false;
    }
    auto candidate=std::move(s.pending);
    auto published=std::move(candidate->scene);
    candidate.reset();
    if(published->allocation)published->allocation->resize(published->bytes);
    if(s.config.admit_publication && !s.config.admit_publication(published,s.error)) {
      s.budget.deferred=s.error.empty();
      if(s.config.scheduler)check(s.config.scheduler->release(&s),"deferred ray scratch fence incomplete");
      return false;
    }
    if(s.current) {
      if(s.current->allocation)s.current->allocation->phase(SceneMemoryPhase::Retired);
      s.retired.emplace_back(s.current);
    }
    published->generation=++s.generation;s.current=std::move(published);
    // All transient buffers and borrowed scratch references are released before
    // shrinking the candidate to its immutable published allocation.
    if(s.current->allocation) {
      s.current->allocation->resize(s.current->bytes);s.current->allocation->phase(SceneMemoryPhase::Resident);
    }
    if(s.config.scheduler)check(s.config.scheduler->release(&s),"scene ray scratch released before build completion");
    return true;
  } catch(const std::exception& e) {s.error=e.what();return false;}
}
bool RayGeometry::reference(std::shared_ptr<const RaySnapshot> scene,rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;if(!scene||!scene->generation||!fence||!value) {s.error="invalid ray snapshot reference";return false;}
  State::Use use;use.scene=std::move(scene);use.fence=fence;use.value=value;s.uses.push_back(std::move(use));s.error.clear();return true;
}
std::shared_ptr<const RaySnapshot> RayGeometry::snapshot() const {return state_->current;}
std::uint64_t RayGeometry::gpu_bytes() const {return state_->live_bytes()+(state_->pending?state_->pending->build_bytes:0);}
const RayGeometryBudget& RayGeometry::budget() const {return state_->budget;}
std::span<const PageHandle> RayGeometry::pending_pages() const {return state_->pending?std::span<const PageHandle>(state_->pending->source_pages):std::span<const PageHandle>{};}
std::span<const PageRequest> RayGeometry::requests() const {return state_->feedback;}
const std::string& RayGeometry::error() const {return state_->error;}
}
