#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <cstdint>

namespace octaryn::client::rendering {

struct WorldRenderer;

// Dormant block-transport lookup. The voxel block-transport GI system is
// archived (see git history); HDR composite, map and reflection shaders still
// carry the lookup parameters, so this binds two small zeroed buffers with
// active=0 exactly like the original inactive path. It is re-founded when the
// new world streaming ships world geometry.
inline constexpr std::uint32_t BlockTransportCapacity = 65536;

struct BlockTransportLookup {
  Slang::ComPtr<rhi::IBuffer> empty_surface, empty_value;
  unsigned epoch{}, radiance_epoch{};
  std::uint64_t gpu_bytes{};
  bool active{};
};

bool bind_block_transport_lookup(WorldRenderer&, rhi::IShaderObject*);

}
