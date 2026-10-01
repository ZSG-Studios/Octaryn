#include "SceneRasterTablesInternal.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <algorithm>
#include <filesystem>

namespace octaryn::client::rendering::virtual_geometry {
SceneRasterTables::SceneRasterTables():state_(std::make_unique<State>()) {}
SceneRasterTables::~SceneRasterTables()=default;
std::uint64_t SceneRasterTables::required_bytes(SceneRasterCapacity c) {
  return scene_raster_bytes(c);
}
bool SceneRasterTables::capacity(std::span<const SceneRasterAsset> assets,SceneRasterCapacity& result) {
  result={};
  for(const auto& asset:assets) {
    if(!asset.clusters || !asset.cluster_count || !asset.page_count || asset.instances.empty())return false;
    result.clusters+=asset.cluster_count;result.pages+=asset.page_count;result.instances+=asset.instances.size();
    result.draws+=std::uint64_t(asset.cluster_count)*asset.instances.size();
    if(required_bytes(result)==UINT64_MAX)return false;
  }
  return !assets.empty();
}
bool SceneRasterTables::initialize(rhi::IDevice* device,std::shared_ptr<SceneMemoryLedger> ledger,const char* directory) {
  auto& s=*state_;if(s.device || !device || !ledger || !directory)return s.fail("invalid scene raster initialization");
  s.device=device;s.ledger=std::move(ledger);
  const auto path=(std::filesystem::path(directory)/"SceneAppend.slang").generic_string();
  return (create_rhi_compute_pipeline(device,path.c_str(),"append_main",s.append) &&
      create_rhi_compute_pipeline(device,path.c_str(),"roots_main",s.roots) &&
      create_rhi_compute_pipeline(device,path.c_str(),"finish_main",s.finish)) || s.fail("scene raster append pipeline failed");
}
bool SceneRasterTables::State::buffer(Slang::ComPtr<rhi::IBuffer>& out,std::uint64_t bytes,unsigned stride,bool readback) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;
  desc.usage=readback?rhi::BufferUsage::CopyDestination:rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|
      rhi::BufferUsage::CopyDestination|rhi::BufferUsage::CopySource|rhi::BufferUsage::IndirectArgument;
  desc.defaultState=readback?rhi::ResourceState::CopyDestination:rhi::ResourceState::ShaderResource;
  if(readback)desc.memoryType=rhi::MemoryType::ReadBack;
  return SLANG_SUCCEEDED(device->createBuffer(desc,nullptr,out.writeRef())) || fail("scene raster table allocation failed");
}
std::shared_ptr<SceneRasterTables::State::Bank> SceneRasterTables::State::allocate(SceneRasterCapacity c) {
  auto result=std::make_shared<Bank>();result->allocation=ledger->reserve(required_bytes(c)/2,SceneMemoryDomain::Selection);
  if(!result->allocation) {admission_rejected=true;fail("complete scene raster tables exceed aggregate GPU budget");return {};}
  if(!buffer(result->clusters,c.clusters*sizeof(GeometryCluster),sizeof(GeometryCluster)) ||
      !buffer(result->pages,c.pages*sizeof(GpuPage),sizeof(GpuPage)) ||
      !buffer(result->instances,c.instances*sizeof(SceneRasterInstance),sizeof(SceneRasterInstance)) ||
      !buffer(result->draws,c.draws*sizeof(SceneRasterDraw),sizeof(SceneRasterDraw)) ||
      !buffer(result->selected,c.draws*16,16) || !buffer(result->counters,24,4) ||
      !buffer(result->dispatch,24,4) || !buffer(result->readback,24,4,true) || !buffer(result->domains,c.clusters*48,48))return {};
  return result;
}
bool SceneRasterTables::State::complete(const Bank& bank) const {
  if(!bank.fence)return true;std::uint64_t value{};
  return SLANG_SUCCEEDED(bank.fence->getCurrentValue(&value)) && value!=UINT64_MAX && value>=bank.signal;
}
bool SceneRasterTables::State::poll(Bank& bank) {
  if(!bank.pending_feedback)return true;
  if(!complete(bank))return fail("scene raster consumer fence is pending");
  void* data{};
  if(SLANG_FAILED(device->mapBuffer(bank.readback,rhi::CpuAccessMode::Read,&data)) || !data)return fail("scene raster feedback mapping failed");
  const auto* words=static_cast<const std::uint32_t*>(data);const bool valid=words[1]==0 && words[0]<=capacity.draws;
  const auto result=device->unmapBuffer(bank.readback);bank.pending_feedback=false;
  return (valid && SLANG_SUCCEEDED(result)) || fail("GPU rejected incomplete scene raster draw domains");
}
bool SceneRasterTables::State::collect() {
  for(auto i=retired.begin();i!=retired.end();) {
    if(!complete(**i)) {++i;continue;}
    if(!poll(**i))return false;i=retired.erase(i);
  }
  return true;
}
bool SceneRasterTables::reserve(SceneRasterCapacity wanted) {
  auto& s=*state_;s.error.clear();s.admission_rejected=false;
  if(!s.device || s.encoder)return s.fail("invalid scene raster reserve lifecycle");
  if(required_bytes(wanted)==UINT64_MAX) {s.admission_rejected=true;return s.fail("complete scene raster cut exceeds identity capacity");}
  if(!s.collect())return false;
  if(wanted.clusters<=s.capacity.clusters && wanted.pages<=s.capacity.pages && wanted.instances<=s.capacity.instances &&
      wanted.draws<=s.capacity.draws)return true;
  wanted.clusters=std::max(wanted.clusters,s.capacity.clusters);wanted.pages=std::max(wanted.pages,s.capacity.pages);
  wanted.instances=std::max(wanted.instances,s.capacity.instances);wanted.draws=std::max(wanted.draws,s.capacity.draws);
  std::array<std::shared_ptr<State::Bank>,2> next;
  for(auto& bank:next)if(!(bank=s.allocate(wanted)))return false;
  s.retired.reserve(s.retired.size()+s.banks.size());
  for(auto& bank:s.banks)if(bank) {bank->allocation->phase(SceneMemoryPhase::Retired);s.retired.push_back(std::move(bank));}
  s.banks=std::move(next);s.capacity=wanted;return s.collect();
}
std::uint64_t SceneRasterTables::gpu_bytes() const {
  const auto& s=*state_;std::uint64_t bytes{};
  for(const auto& bank:s.banks)if(bank)bytes+=bank->allocation->bytes();
  for(const auto& bank:s.retired)bytes+=bank->allocation->bytes();return bytes;
}
const std::string& SceneRasterTables::error() const {return state_->error;}
bool SceneRasterTables::admission_rejected() const {return state_->admission_rejected;}
}
