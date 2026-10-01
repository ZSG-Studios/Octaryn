#include "SceneMemoryLedger.h"
#include <algorithm>
#include <mutex>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneMemoryState {mutable std::mutex mutex;SceneMemoryStats stats;};
namespace {
unsigned index(SceneMemoryDomain value) {return static_cast<unsigned>(value);}
unsigned index(SceneMemoryPhase value) {return static_cast<unsigned>(value);}
void valid(SceneMemoryDomain domain,SceneMemoryPhase phase) {
  if(index(domain)>=index(SceneMemoryDomain::Count) || index(phase)>=index(SceneMemoryPhase::Count))
    throw std::invalid_argument("invalid scene memory category");
}
}
SceneMemoryLedger::SceneMemoryLedger(std::uint64_t limit):state_(std::make_shared<SceneMemoryState>()) {
  if(!limit)throw std::invalid_argument("empty scene memory budget");
  state_->stats.limit=limit;
}
SceneMemoryLease::SceneMemoryLease(std::shared_ptr<SceneMemoryState> state,std::uint64_t bytes,
    SceneMemoryDomain domain,SceneMemoryPhase phase):state_(std::move(state)),bytes_(bytes),domain_(domain),phase_(phase) {}
SceneMemoryLease::~SceneMemoryLease() {resize(0);}
std::shared_ptr<SceneMemoryLease> SceneMemoryLedger::reserve(std::uint64_t bytes,
    SceneMemoryDomain domain,SceneMemoryPhase phase) {
  valid(domain,phase);
  auto lease=std::shared_ptr<SceneMemoryLease>(new SceneMemoryLease(state_,0,domain,phase));
  return lease->resize(bytes)?lease:nullptr;
}
bool SceneMemoryLease::resize(std::uint64_t bytes) {
  std::lock_guard lock(state_->mutex);auto& stats=state_->stats;
  if(bytes>bytes_ && bytes-bytes_>stats.limit-stats.used)return false;
  stats.used=stats.used-bytes_+bytes;
  stats.domain_bytes[index(domain_)]=stats.domain_bytes[index(domain_)]-bytes_+bytes;
  stats.phase_bytes[index(phase_)]=stats.phase_bytes[index(phase_)]-bytes_+bytes;
  stats.peak=std::max(stats.peak,stats.used);bytes_=bytes;return true;
}
std::shared_ptr<SceneMemoryLease> SceneMemoryLease::split(std::uint64_t bytes,SceneMemoryPhase phase) {
  valid(domain_,phase);
  auto next=std::shared_ptr<SceneMemoryLease>(new SceneMemoryLease(state_,0,domain_,phase));
  std::lock_guard lock(state_->mutex);if(bytes>bytes_)return {};
  bytes_-=bytes;next->bytes_=bytes;
  state_->stats.phase_bytes[index(phase_)]-=bytes;state_->stats.phase_bytes[index(phase)]+=bytes;
  return next;
}
void SceneMemoryLease::phase(SceneMemoryPhase phase) {
  valid(domain_,phase);std::lock_guard lock(state_->mutex);auto& stats=state_->stats;
  stats.phase_bytes[index(phase_)]-=bytes_;stats.phase_bytes[index(phase)]+=bytes_;phase_=phase;
}
std::uint64_t SceneMemoryLease::bytes() const {std::lock_guard lock(state_->mutex);return bytes_;}
SceneMemoryStats SceneMemoryLedger::stats() const {std::lock_guard lock(state_->mutex);return state_->stats;}
bool SceneMemoryLedger::owns(const std::shared_ptr<SceneMemoryLease>& lease) const {return lease && lease->state_==state_;}
}
