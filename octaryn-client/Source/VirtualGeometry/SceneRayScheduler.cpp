#include "SceneRayScheduler.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include "../Rendering/RenderBackend/DeviceMemory.h"
#include <algorithm>
#include <deque>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneRayScheduler::State {
  std::shared_ptr<SceneMemoryLedger> ledger;
  std::shared_ptr<SceneMemoryLease> scratch_lease;
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IComputePipeline> expansion;
  Slang::ComPtr<rhi::IBuffer> scratch;
  Slang::ComPtr<rhi::IFence> fence;
  std::deque<const void*> waiting;
  const void* owner{};std::uint64_t signal{};
  std::string error;
  bool idle() {
    if(!fence)return true;
    std::uint64_t completed{};
    if(SLANG_FAILED(fence->getCurrentValue(&completed)) || completed==UINT64_MAX) {
      error="scene ray scratch fence failed";return false;
    }
    if(completed<signal)return false;
    fence.setNull();signal=0;return true;
  }
};
SceneRayScheduler::SceneRayScheduler(std::shared_ptr<SceneMemoryLedger> ledger):state_(std::make_unique<State>()) {
  if(!ledger)throw std::invalid_argument("scene ray scheduler requires an aggregate ledger");
  state_->ledger=std::move(ledger);
}
SceneRayScheduler::~SceneRayScheduler()=default;
bool SceneRayScheduler::initialize(rhi::IDevice* device,const char* shader) {
  auto& s=*state_;
  if(s.device && s.device.get()!=device) {s.error="scene ray scheduler device changed";return false;}
  if(s.expansion)return true;
  if(!device || !shader || !create_rhi_compute_pipeline(device,shader,"expandRayGeometry",s.expansion)) {
    s.error="scene shared ray expansion pipeline failed";return false;
  }
  s.device=device;s.error.clear();return true;
}
bool SceneRayScheduler::acquire(const void* owner) {
  auto& s=*state_;if(!owner)return false;
  if(s.owner==owner)return true;
  if(std::find(s.waiting.begin(),s.waiting.end(),owner)==s.waiting.end())s.waiting.push_back(owner);
  if(s.owner || s.waiting.front()!=owner || !s.idle())return false;
  s.waiting.pop_front();s.owner=owner;return true;
}
bool SceneRayScheduler::release(const void* owner) {
  auto& s=*state_;std::erase(s.waiting,owner);
  if(s.owner!=owner)return true;
  if(!s.idle())return false;
  s.owner=nullptr;return true;
}
bool SceneRayScheduler::submitted(const void* owner,rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;
  if(s.owner!=owner || !fence || !value || !s.idle()) {s.error="invalid scene ray scratch submission";return false;}
  s.fence=fence;s.signal=value;return true;
}
std::shared_ptr<SceneMemoryLease> SceneRayScheduler::reserve(const void* owner,
    std::uint64_t bytes,std::uint64_t scratch_bytes) {
  auto& s=*state_;s.error.clear();
  if(s.owner!=owner || !s.device || !s.idle()) {s.error="scene ray build does not own idle scratch";return {};}
  const auto previous=s.scratch?s.scratch->getDesc().size:0;
  const auto growth=scratch_bytes>previous?scratch_bytes-previous:0;
  const auto stats=s.ledger->stats();
  if(bytes>stats.limit-stats.used || growth>stats.limit-stats.used-bytes)return {};
  const auto memory=device_memory_stats(s.device->getInfo(),true);
  if(memory.budget_available && (memory.local_usage>=memory.local_budget ||
      bytes+growth>memory.local_budget-memory.local_usage)) {
    s.error="complete scene ray candidate exceeds available device budget";return {};
  }
  auto allocation=s.ledger->reserve(bytes,SceneMemoryDomain::RayGeometry,SceneMemoryPhase::Pending);
  if(!allocation)return {};
  if(growth) {
    // Exclusive ownership and the completed fence permit releasing old capacity.
    s.scratch.setNull();
    if(!s.scratch_lease)s.scratch_lease=s.ledger->reserve(0,SceneMemoryDomain::RayScratch);
    if(!s.scratch_lease->resize(scratch_bytes))return {};
    rhi::BufferDesc desc{};desc.size=scratch_bytes;desc.elementSize=4;
    desc.usage=rhi::BufferUsage::UnorderedAccess;desc.defaultState=rhi::ResourceState::UnorderedAccess;
    if(SLANG_FAILED(s.device->createBuffer(desc,nullptr,s.scratch.writeRef()))) {
      s.scratch_lease->resize(0);s.error="scene shared ray scratch allocation failed";return {};
    }
  }
  return allocation;
}
rhi::IBuffer* SceneRayScheduler::scratch() const {return state_->scratch;}
rhi::IComputePipeline* SceneRayScheduler::expansion() const {return state_->expansion;}
std::shared_ptr<SceneMemoryLedger> SceneRayScheduler::ledger() const {return state_->ledger;}
const std::string& SceneRayScheduler::error() const {return state_->error;}
}
