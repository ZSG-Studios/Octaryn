#include "MapSamplerCache.h"
#include <bit>
#include <cmath>
#include <cstdio>

namespace octaryn::client::rendering {
bool map_sampler_key(const rhi::SamplerDesc& desc,MapSamplerKey& key) {
  if(desc.structType!=rhi::StructType::SamplerDesc || desc.next)return false;
  for(float value:{desc.mipLODBias,desc.borderColor[0],desc.borderColor[1],desc.borderColor[2],
      desc.borderColor[3],desc.minLOD,desc.maxLOD})if(!std::isfinite(value))return false;
  // Field-wise identity excludes padding and the nonfunctional debug label.
  key={unsigned(desc.minFilter),unsigned(desc.magFilter),unsigned(desc.mipFilter),unsigned(desc.reductionOp),
      unsigned(desc.addressU),unsigned(desc.addressV),unsigned(desc.addressW),std::bit_cast<std::uint32_t>(desc.mipLODBias),
      desc.maxAnisotropy,unsigned(desc.comparisonFunc),std::bit_cast<std::uint32_t>(desc.borderColor[0]),
      std::bit_cast<std::uint32_t>(desc.borderColor[1]),std::bit_cast<std::uint32_t>(desc.borderColor[2]),
      std::bit_cast<std::uint32_t>(desc.borderColor[3]),std::bit_cast<std::uint32_t>(desc.minLOD),
      std::bit_cast<std::uint32_t>(desc.maxLOD)};
  return true;
}
std::shared_ptr<MapSamplerResource> acquire_map_sampler(MapSamplerCache& cache,rhi::IDevice* device,
    const rhi::SamplerDesc& desc) {
  MapSamplerKey key;
  if(!device || (cache.device && cache.device.get()!=device) || !map_sampler_key(desc,key)) {
    std::fprintf(stderr,"map_sampler_failed operation=key reason=invalid_descriptor_or_device\n");return {};
  }
  const auto found=cache.entries.find(key);
  if(found!=cache.entries.end())if(auto resource=found->second.lock())return resource;
  for(auto entry=cache.entries.begin();entry!=cache.entries.end();)
    if(entry->second.expired())entry=cache.entries.erase(entry);else ++entry;
  if(cache.entries.size()>=MapSamplerCache::capacity) {
    std::fprintf(stderr,"map_sampler_failed operation=reserve live_configs=%zu limit=%zu reason=map_sampler_budget\n",
        cache.entries.size(),MapSamplerCache::capacity);return {};
  }
  auto resource=std::make_shared<MapSamplerResource>();
  auto result=device->createSampler(desc,resource->sampler.writeRef());
  const char* operation="create";
  if(SLANG_SUCCEEDED(result)) {operation="bindless_descriptor";result=resource->sampler->getDescriptorHandle(&resource->descriptor);}
  if(SLANG_FAILED(result)) {
    std::fprintf(stderr,"map_sampler_failed operation=%s result=0x%08x live_configs=%zu map_limit=%zu wrap=%u,%u,%u filters=%u,%u,%u anisotropy=%u\n",
        operation,unsigned(result),cache.entries.size(),MapSamplerCache::capacity,unsigned(desc.addressU),unsigned(desc.addressV),
        unsigned(desc.addressW),unsigned(desc.minFilter),unsigned(desc.magFilter),unsigned(desc.mipFilter),desc.maxAnisotropy);
    return {};
  }
  cache.device=device;cache.entries[key]=resource;
  std::printf("map_sampler_cache live_configs=%zu limit=%zu\n",cache.entries.size(),MapSamplerCache::capacity);
  return resource;
}
}
