#include "MapSamplerCache.h"
#include <cstdio>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

using namespace octaryn::client::rendering;
int main() {
  const auto require=[](bool okay) {if(!okay)throw std::runtime_error("sampler descriptor identity regression");};
  rhi::SamplerDesc original{};MapSamplerKey reference{};
  require(map_sampler_key(original,reference));
  const std::vector<std::function<void(rhi::SamplerDesc&)>> changes{
      [](auto& d){d.minFilter=rhi::TextureFilteringMode::Point;},
      [](auto& d){d.magFilter=rhi::TextureFilteringMode::Point;},
      [](auto& d){d.mipFilter=rhi::TextureFilteringMode::Point;},
      [](auto& d){d.reductionOp=rhi::TextureReductionOp::Minimum;},
      [](auto& d){d.addressU=rhi::TextureAddressingMode::ClampToEdge;},
      [](auto& d){d.addressV=rhi::TextureAddressingMode::ClampToEdge;},
      [](auto& d){d.addressW=rhi::TextureAddressingMode::ClampToEdge;},
      [](auto& d){d.mipLODBias=.5f;},[](auto& d){d.maxAnisotropy=8;},
      [](auto& d){d.comparisonFunc=rhi::ComparisonFunc::Less;},
      [](auto& d){d.borderColor[0]=1;},[](auto& d){d.borderColor[1]=1;},
      [](auto& d){d.borderColor[2]=1;},[](auto& d){d.borderColor[3]=1;},
      [](auto& d){d.minLOD=1;},[](auto& d){d.maxLOD=0;}};
  std::set<MapSamplerKey> distinct{reference};
  for(const auto& change:changes) {
    auto descriptor=original;change(descriptor);MapSamplerKey key{};
    require(map_sampler_key(descriptor,key) && key!=reference && distinct.insert(key).second);
  }
  for(unsigned tile=0;tile<137;++tile) {
    auto descriptor=original;descriptor.label="tile label";MapSamplerKey key{};
    require(map_sampler_key(descriptor,key) && key==reference);
  }
  auto invalid=original;invalid.next=&original;MapSamplerKey key{};
  require(!map_sampler_key(invalid,key));
  invalid=original;invalid.minLOD=std::numeric_limits<float>::quiet_NaN();
  require(!map_sampler_key(invalid,key));
  std::printf("map_sampler_tests passed=1 descriptor_fields=16 repeated_tiles=137 invalid_extensions=1\n");
}
