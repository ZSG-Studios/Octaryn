#include "SceneGeometryPoolInternal.h"
#include <algorithm>
#include <chrono>
#include <cfloat>

namespace octaryn::client::rendering::virtual_geometry {
bool SceneGeometryPool::record(rhi::ICommandEncoder* commands,SceneGeometryHandle id,std::span<const PageRequest> requests) {
  auto& s=*state_;auto* asset=s.find(id);
  if(s.failed)return false;
  if(!commands || !asset || asset->retiring || (s.encoder && s.encoder!=commands))
    return s.fail("shared geometry requires one frame encoder and a live asset");
  if(!s.encoder) {
    if(!s.poll())return false;
    if(s.epoch==UINT64_MAX)return s.fail("shared geometry frame generation exhausted");
    s.encoder=commands;++s.epoch;s.counters.uploaded_this_frame=0;
    s.uploads.clear();s.references.clear();s.consumers.clear();s.root_uploads.clear();s.root_references.clear();
    std::fill(s.reference_generations.begin(),s.reference_generations.end(),0);
    const auto start=std::chrono::steady_clock::now();
    for(auto& pointer:s.jobs) {
      auto& job=*pointer;if(!job.ready)continue;
      if(s.counters.uploaded_this_frame>=s.config.upload_pages ||
          std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>=s.config.upload_ms)break;
      if(job.root?!s.root_pages->valid(job.root):!s.residency->valid(job.handle))return s.fail("shared geometry upload reservation became stale");
      const auto offset=job.root?std::uint64_t(job.root.slot)*page_bytes+job.root.offset:
          std::uint64_t(job.handle.slot+s.config.root_slots)*page_bytes;
      const auto bytes=job.root?job.root.bytes:page_bytes;
      if(SLANG_FAILED(commands->uploadBufferData(s.pool,offset,bytes,job.decoded.data())))
        return s.fail("shared geometry upload recording failed");
      if(job.root)s.root_uploads.push_back(job.page);else s.uploads.push_back(job.handle);
      job.ready=false;job.decoded.clear();
      ++s.counters.uploaded_this_frame;++s.counters.uploaded_pages;s.counters.uploaded_bytes+=bytes;
    }
    if(!s.uploads.empty() || !s.root_uploads.empty())commands->setBufferState(s.pool,rhi::ResourceState::ShaderResource);
    // Reserve/evict before any draw has captured this frame's local page tables.
    if(!s.start_jobs())return false;
  }
  std::vector<PageRequest> mapped;mapped.reserve(std::min<std::size_t>(requests.size(),s.config.feedback_capacity));
  // Root admission can exceed the bounded request queue; retry omitted roots until decoded.
  for(const auto page:asset->roots) {
    const auto& owner=s.owners[page];if(owner.loading || owner.upload_signal)continue;
    mapped.push_back({page,FLT_MAX});
    if(mapped.size()==s.config.feedback_capacity) {s.residency->feedback(mapped);mapped.clear();}
  }
  for(const auto request:requests) {
    if(request.page>=asset->pages.size())return s.fail("shared geometry feedback page outside asset");
    const auto global=asset->pages[request.page];const auto& owner=s.owners[global];
    if(owner.root && (owner.loading || owner.upload_signal))continue;
    mapped.push_back({global,request.priority});
    if(mapped.size()==s.config.feedback_capacity) {s.residency->feedback(mapped);mapped.clear();}
  }
  s.residency->feedback(mapped);
  if(asset->recorded_epoch!=s.epoch) {
    asset->recorded_epoch=s.epoch;s.consumers.push_back(id);
    for(const auto page:asset->pages) {
      const auto& owner=s.owners[page];
      if(owner.root) {
        if(owner.upload_signal && owner.upload_signal<=s.completed)s.root_references.push_back(page);
        continue;
      }
      if(!s.residency->resident(page))continue;
      const auto h=s.residency->handle(page);
      if(s.reference_generations[h.slot]==h.generation)continue;
      s.reference_generations[h.slot]=h.generation;s.references.push_back(h);
    }
  }
  return true;
}
bool SceneGeometryPool::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;
  if(!fence || !value || (s.fence && s.fence.get()!=fence))return s.fail("shared geometry fence timeline changed");
  if(!s.encoder)return (s.fence.get()==fence && s.signal==value) || s.fail("shared geometry has no recorded frame");
  if(value<=s.signal)return s.fail("shared geometry fence did not advance");
  s.fence=fence;s.signal=value;
  for(const auto h:s.uploads)if(!s.residency->begin_upload(h,value))return s.fail("shared geometry upload publication failed");
  for(auto page:s.root_uploads) {
    auto& owner=s.owners[page];
    if(!owner.root || owner.upload_signal)return s.fail("packed root upload publication mismatch");
    owner.upload_signal=value;
  }
  for(const auto h:s.references)if(!s.residency->reference(h,{0,value,0,0}))return s.fail("shared geometry consumer reference stale");
  for(auto page:s.root_references)s.owners[page].consumer_signal=value;
  for(const auto id:s.consumers)if(auto* asset=s.find(id))asset->last_signal=value;
  s.uploads.clear();s.references.clear();s.consumers.clear();s.root_uploads.clear();s.root_references.clear();s.encoder=nullptr;return true;
}
}
