#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>
#include <map>
#include <memory>

namespace octaryn::client::rendering {
using MapSamplerKey=std::array<std::uint32_t,16>;
bool map_sampler_key(const rhi::SamplerDesc&,MapSamplerKey&);
struct MapSamplerResource {
  Slang::ComPtr<rhi::ISampler> sampler;
  rhi::DescriptorHandle descriptor;
};
// Render-owner only. Maps retain resources until their last submitted frame retires.
struct MapSamplerCache {
  static constexpr std::size_t capacity=64;
  Slang::ComPtr<rhi::IDevice> device;
  std::map<MapSamplerKey,std::weak_ptr<MapSamplerResource>> entries;
};
std::shared_ptr<MapSamplerResource> acquire_map_sampler(MapSamplerCache&,rhi::IDevice*,const rhi::SamplerDesc&);
}
