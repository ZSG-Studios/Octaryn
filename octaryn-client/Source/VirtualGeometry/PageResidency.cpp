#include "PageResidency.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void advance(FenceValues& a, FenceValues b) {
  a.upload=std::max(a.upload,b.upload);a.raster=std::max(a.raster,b.raster);
  a.ray=std::max(a.ray,b.ray);a.pose=std::max(a.pose,b.pose);
}
bool passed(FenceValues wanted, FenceValues done) {
  return wanted.upload<=done.upload && wanted.raster<=done.raster &&
      wanted.ray<=done.ray && wanted.pose<=done.pose;
}
}
PageResidency::PageResidency(std::uint32_t pages,std::uint32_t slots,std::uint64_t budget,
    std::uint32_t capacity):slots_(slots),pages_(pages),budget_(budget),feedback_capacity_(capacity) {
  if(!pages || !slots || !budget || !capacity)throw std::invalid_argument("empty geometry residency limits");
  requests_.reserve(capacity);
}
bool PageResidency::valid(PageHandle h) const {
  return h && h.slot<slots_.size() && slots_[h.slot].generation==h.generation &&
      slots_[h.slot].state!=PageState::Free;
}
PageHandle PageResidency::handle(std::uint32_t page) const {
  return page<pages_.size()?pages_[page]:PageHandle{};
}
PageHandle PageResidency::reserve(std::uint32_t page,std::uint64_t bytes,bool pin_value) {
  if(page>=pages_.size() || !bytes)return {};
  if(auto h=handle(page);valid(h)) {
    auto& slot=slots_[h.slot];
    if(slot.state==PageState::Retiring || slot.bytes!=bytes)return {};
    slot.pinned|=pin_value;slot.touched=++clock_;return h;
  }
  if(bytes>budget_-bytes_)return {};
  for(std::uint32_t i=0;i<slots_.size();++i) {
    auto& slot=slots_[i];
    // Exhausted generations never wrap into a stale handle.
    if(slot.state!=PageState::Free || slot.generation==std::numeric_limits<std::uint32_t>::max())continue;
    const auto generation=slot.generation+1;
    slot={page,generation,PageState::Reserved,pin_value,bytes,++clock_,{}};
    bytes_+=bytes;return pages_[page]={i,generation};
  }
  return {};
}
bool PageResidency::begin_upload(PageHandle h,std::uint64_t fence) {
  if(!valid(h) || !fence || fence<=completed_.upload)return false;
  auto& slot=slots_[h.slot];if(slot.state!=PageState::Reserved)return false;
  slot.fences.upload=fence;slot.state=PageState::Uploading;return true;
}
bool PageResidency::reference(PageHandle h,FenceValues fences) {
  if(!valid(h))return false;
  auto& slot=slots_[h.slot];if(slot.state!=PageState::Resident)return false;
  advance(slot.fences,fences);slot.touched=++clock_;return true;
}
bool PageResidency::pin(std::uint32_t page,bool value) {
  auto h=handle(page);if(!valid(h) || slots_[h.slot].state==PageState::Retiring)return false;
  slots_[h.slot].pinned=value;return true;
}
bool PageResidency::evict(std::uint32_t page) {
  auto h=handle(page);if(!valid(h))return false;
  auto& slot=slots_[h.slot];if(slot.pinned || slot.state==PageState::Retiring)return false;
  slot.state=PageState::Retiring;++evictions_;
  // Keep the mapping until retirement so the same page cannot be re-admitted.
  complete(completed_);return true;
}
bool PageResidency::evict_oldest() {
  auto candidate=invalid_page;auto oldest=std::numeric_limits<std::uint64_t>::max();
  for(const auto& slot:slots_)if(!slot.pinned && slot.state==PageState::Resident && slot.touched<oldest) {
    candidate=slot.page;oldest=slot.touched;
  }
  return candidate!=invalid_page && evict(candidate);
}
void PageResidency::complete(FenceValues values) {
  advance(completed_,values);
  for(auto& slot:slots_) {
    if(slot.state==PageState::Uploading && slot.fences.upload<=completed_.upload)slot.state=PageState::Resident;
    if(slot.state!=PageState::Retiring || !passed(slot.fences,completed_))continue;
    pages_[slot.page]={};bytes_-=slot.bytes;
    const auto generation=slot.generation;slot={};slot.generation=generation;
  }
}
bool PageResidency::resident(std::uint32_t page) const {
  auto h=handle(page);return valid(h) && slots_[h.slot].state==PageState::Resident;
}
std::vector<GpuPage> PageResidency::page_table() const {
  std::vector<GpuPage> result(pages_.size());
  for(std::uint32_t page=0;page<pages_.size();++page)if(auto h=handle(page);valid(h))
    result[page]={h.slot,h.generation,resident(page)?1u:0u,0};
  return result;
}
void PageResidency::feedback(std::span<const PageRequest> values) {
  for(auto request:values) {
    if(request.page>=pages_.size() || !std::isfinite(request.priority) || request.priority<0) {++invalid_;continue;}
    if(valid(handle(request.page)))continue;
    auto prior=std::find_if(requests_.begin(),requests_.end(),[&](auto p){return p.page==request.page;});
    if(prior!=requests_.end()) {prior->priority=std::max(prior->priority,request.priority);continue;}
    if(requests_.size()<feedback_capacity_) {requests_.push_back(request);continue;}
    ++overflow_;
    auto low=std::min_element(requests_.begin(),requests_.end(),[](auto a,auto b){return a.priority<b.priority;});
    if(request.priority>low->priority)*low=request;
  }
}
std::vector<PageRequest> PageResidency::take_requests(std::uint32_t maximum) {
  std::erase_if(requests_,[&](auto request){return valid(handle(request.page));});
  std::sort(requests_.begin(),requests_.end(),[](auto a,auto b) {
    return a.priority==b.priority?a.page<b.page:a.priority>b.priority;
  });
  const auto count=std::min<std::size_t>(maximum,requests_.size());
  std::vector<PageRequest> result(requests_.begin(),requests_.begin()+count);
  requests_.erase(requests_.begin(),requests_.begin()+count);return result;
}
ResidencyStats PageResidency::stats() const {
  ResidencyStats result{bytes_,budget_,overflow_,invalid_,evictions_};
  for(const auto& slot:slots_) {
    result.resident+=slot.state==PageState::Resident;
    result.pending+=slot.state==PageState::Reserved || slot.state==PageState::Uploading;
    result.retiring+=slot.state==PageState::Retiring;
  }
  return result;
}
}
