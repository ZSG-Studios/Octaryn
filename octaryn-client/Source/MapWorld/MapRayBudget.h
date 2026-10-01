#pragma once
#include <cstdint>
namespace rhi {class IDevice;class IBuffer;}
namespace octaryn::client::rendering {
struct MapRenderer;
struct MapModel;
// Maximum live AS storage across build and compaction, excluding driver heaps.
bool map_ray_build_budget(const MapRenderer&,std::uint64_t& peak_bytes);
// Size queries use only counts/layout. The persistent address buffer satisfies
// the RHI's nonnull input contract; no GPU command accesses its contents.
bool map_ray_prepare_budget(rhi::IDevice*,rhi::IBuffer* address_buffer,const MapModel&,std::uint64_t& peak_bytes);
}
