#include "SceneRasterTablesInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstring>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
bool bind(rhi::IShaderObject* root,const char* name,rhi::IBuffer* buffer) {
  auto field=rhi::ShaderCursor(root)[name];return !field.isValid() || SLANG_SUCCEEDED(field.setBinding(rhi::Binding(buffer)));
}
bool data(rhi::IShaderObject* root,const char* name,const void* value,std::size_t size) {
  auto field=rhi::ShaderCursor(root)[name];return !field.isValid() || SLANG_SUCCEEDED(field.setData(value,size));
}
}
bool SceneRasterTables::begin(rhi::ICommandEncoder* commands,std::uint32_t slot,std::span<const SceneRasterAsset> assets,
    SceneRasterFrame& output) {
  auto& s=*state_;output={};s.error.clear();SceneRasterCapacity wanted;
  if(!commands || s.encoder || slot>=s.banks.size() || !s.banks[slot] || !capacity(assets,wanted) ||
      wanted.clusters>s.capacity.clusters || wanted.pages>s.capacity.pages || wanted.instances>s.capacity.instances ||
      wanted.draws>s.capacity.draws || s.generation==UINT32_MAX)return s.fail("scene raster complete cut was not admitted");
  auto& bank=*s.banks[slot];
  if(!s.complete(bank) || !s.poll(bank) || !s.collect())return s.fail("scene raster bank still has consumers or invalid feedback");
  s.domains.clear();s.domains.reserve(assets.size());std::vector<SceneRasterInstance> transforms;transforms.reserve(wanted.instances);
  State::Domain offset;
  for(const auto& asset:assets) {
    if(asset.clusters->getDesc().size<std::uint64_t(asset.cluster_count)*sizeof(GeometryCluster))
      return s.fail("scene raster cluster metadata range is invalid");
    auto domain=offset;domain.instances=unsigned(asset.instances.size());domain.clusters=asset.cluster_count;
    domain.pages=asset.page_count;domain.material=asset.material_base;s.domains.push_back(domain);
    for(const auto& instance:asset.instances)transforms.push_back({instance.world,instance.normal,instance.orientation});
    offset.cluster_base+=asset.cluster_count;offset.page_base+=asset.page_count;
    offset.instance_base+=unsigned(asset.instances.size());offset.draw_base+=unsigned(std::uint64_t(asset.cluster_count)*asset.instances.size());
  }
  bank.generation=++s.generation;s.encoder=commands;s.active=slot;s.sealed=false;
  output={bank.clusters,bank.pages,bank.instances,bank.draws,bank.selected,bank.counters,bank.dispatch,
      bank.generation,unsigned(s.capacity.draws),slot};
  commands->globalBarrier();const std::uint32_t zero[6]{};
  if(SLANG_FAILED(commands->uploadBufferData(bank.counters,0,sizeof(zero),zero)) ||
      SLANG_FAILED(commands->uploadBufferData(bank.instances,0,transforms.size()*sizeof(SceneRasterInstance),transforms.data())))
    return s.fail("scene raster frame table upload failed");
  for(unsigned i=0;i<assets.size();++i)commands->copyBuffer(bank.clusters,
      std::uint64_t(s.domains[i].cluster_base)*sizeof(GeometryCluster),assets[i].clusters,0,
      std::uint64_t(assets[i].cluster_count)*sizeof(GeometryCluster));
  return true;
}
bool SceneRasterTables::append(rhi::ICommandEncoder* commands,std::uint32_t asset,const SelectionGpuFrame& selection) {
  auto& s=*state_;
  if(commands!=s.encoder || s.sealed || asset>=s.domains.size() || s.domains[asset].appended ||
      !selection.selected || !selection.counters || !selection.page_table)return s.fail("invalid scene raster append lifecycle");
  auto& domain=s.domains[asset];auto& bank=*s.banks[s.active];
  const auto page_bytes=std::uint64_t(domain.pages)*sizeof(GpuPage);
  if(selection.page_table->getDesc().size<page_bytes)return s.fail("scene raster local page table is too small");
  commands->globalBarrier();commands->copyBuffer(bank.pages,std::uint64_t(domain.page_base)*sizeof(GpuPage),selection.page_table,0,page_bytes);
  auto* pass=commands->beginComputePass();if(!pass)return s.fail("scene raster append pass failed");
  auto* root=pass->bindPipeline(s.append);const std::uint32_t limits[]{unsigned(s.capacity.draws),bank.generation};
  bool ok=root && bind(root,"sceneLocalSelected",selection.selected) && bind(root,"sceneLocalCounters",selection.counters) &&
      bind(root,"sceneDraws",bank.draws) && bind(root,"sceneSelected",bank.selected) && bind(root,"sceneCounters",bank.counters) &&
      data(root,"sceneDomain",&domain,8*sizeof(std::uint32_t)) && data(root,"sceneLimits",limits,sizeof(limits));
  const auto threads=std::uint64_t(domain.clusters)*domain.instances;const auto groups=unsigned((threads+63)/64);
  if(ok)pass->dispatchCompute(std::min(groups,65535u),std::max(1u,(groups+65534)/65535),1);
  pass->end();commands->globalBarrier();domain.appended=ok;
  return ok || s.fail("scene raster append binding failed");
}
bool SceneRasterTables::finish(rhi::ICommandEncoder* commands) {
  auto& s=*state_;
  if(commands!=s.encoder || s.sealed || std::any_of(s.domains.begin(),s.domains.end(),[](const auto& d){return !d.appended;}))
    return s.fail("scene raster did not append every admitted domain");
  auto& bank=*s.banks[s.active];std::vector<std::array<std::uint32_t,12>> domains(s.domains.size());
  for(unsigned i=0;i<domains.size();++i)std::memcpy(domains[i].data(),&s.domains[i],48);
  if(SLANG_FAILED(commands->uploadBufferData(bank.domains,0,domains.size()*48,domains.data())))return s.fail("scene raster domain upload failed");
  const std::uint32_t domain_count=unsigned(domains.size());
  const auto& last=s.domains.back();const auto draws=last.draw_base+last.clusters*last.instances;
  auto* pass=commands->beginComputePass();if(!pass)return s.fail("scene raster roots pass failed");
  auto* root=pass->bindPipeline(s.roots);const std::uint32_t limits[]{unsigned(s.capacity.draws),bank.generation};
  bool ok=root && bind(root,"sceneDomains",bank.domains) && bind(root,"sceneClusters",bank.clusters) &&
      bind(root,"scenePages",bank.pages) && bind(root,"sceneDraws",bank.draws) && bind(root,"sceneSelected",bank.selected) &&
      bind(root,"sceneCounters",bank.counters) && data(root,"sceneLimits",limits,sizeof(limits)) &&
      data(root,"sceneDomainCount",&domain_count,sizeof(domain_count));
  const auto groups=(draws+63)/64;
  if(ok)pass->dispatchCompute(std::min(groups,65535u),std::max(1u,(groups+65534)/65535),1);
  pass->end();commands->globalBarrier();if(!ok)return s.fail("scene raster roots binding failed");
  pass=commands->beginComputePass();
  if(!pass)return s.fail("scene raster finish pass failed");root=pass->bindPipeline(s.finish);
  ok=root && bind(root,"sceneCounters",bank.counters) && bind(root,"sceneDispatch",bank.dispatch) &&
      data(root,"sceneLimits",limits,sizeof(limits));
  if(ok)pass->dispatchCompute(1,1,1);pass->end();commands->globalBarrier();
  if(!ok)return s.fail("scene raster finish binding failed");
  commands->copyBuffer(bank.readback,0,bank.counters,0,24);s.sealed=true;return true;
}
bool SceneRasterTables::append_roots(rhi::ICommandEncoder* commands,std::uint32_t asset,std::span<const GpuPage> pages) {
  auto& s=*state_;
  if(commands!=s.encoder || s.sealed || asset>=s.domains.size() || s.domains[asset].appended ||
      pages.size()!=s.domains[asset].pages)return s.fail("invalid scene root domain");
  for(const auto& page:pages)if(!page.resident || !page.generation || page.slot==invalid_page)
    return s.fail("scene root cut is incomplete");
  auto& domain=s.domains[asset];auto& bank=*s.banks[s.active];
  if(SLANG_FAILED(commands->uploadBufferData(bank.pages,std::uint64_t(domain.page_base)*sizeof(GpuPage),pages.size_bytes(),pages.data())))
    return s.fail("scene root page table upload failed");
  domain.root_cut=1;domain.appended=true;return true;
}
bool SceneRasterTables::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;
  if(!s.encoder)return true;
  if(!s.sealed || !fence || !value)return s.fail("scene raster submission lacks complete draw cut");
  auto& bank=*s.banks[s.active];
  if(bank.fence && (bank.fence.get()!=fence || value<=bank.signal))return s.fail("scene raster frame timeline changed");
  bank.fence=fence;bank.signal=value;bank.pending_feedback=true;s.encoder=nullptr;return true;
}
}
