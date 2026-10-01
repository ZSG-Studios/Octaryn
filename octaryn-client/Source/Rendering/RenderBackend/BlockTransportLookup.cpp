#include "BlockTransportLookup.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <array>

namespace octaryn::client::rendering {

bool bind_block_transport_lookup(WorldRenderer& r, rhi::IShaderObject* root) {
  if (!root) return false;
  rhi::ShaderCursor c(root);
  auto field = c["btLookupInfo"];
  if (!field.isValid()) return true;
  auto& s = r.block_transport_lookup;
  if (!s.empty_surface || !s.empty_value) {
    const std::array<std::uint32_t, 16> zero{};
    auto create = [&](Slang::ComPtr<rhi::IBuffer>& value, unsigned size) {
      if (value) return true;
      rhi::BufferDesc desc{};
      desc.size = size;
      desc.elementSize = size;
      desc.label = "block_transport_empty_lookup";
      desc.defaultState = rhi::ResourceState::ShaderResource;
      desc.usage = rhi::BufferUsage::ShaderResource;
      return world_rhi_ok(r.device->createBuffer(desc, zero.data(), value.writeRef()));
    };
    if (!create(s.empty_surface, 64) || !create(s.empty_value, 16)) return false;
    s.gpu_bytes = 80;
  }
  const std::array<unsigned, 4> info{0u, s.epoch, s.radiance_epoch, BlockTransportCapacity};
  auto bind = [&](const char* name, rhi::IBuffer* value) {
    auto target = c[name];
    return !target.isValid() || world_rhi_ok(target.setBinding(rhi::Binding(value)));
  };
  return world_rhi_ok(field.setData(info.data(), sizeof(info))) &&
      bind("btLookupSurfaces", s.empty_surface.get()) &&
      bind("btLookupDirect", s.empty_value.get()) &&
      bind("btLookupIndirect", s.empty_value.get()) &&
      bind("btLookupEnvironment", s.empty_value.get());
}

}
