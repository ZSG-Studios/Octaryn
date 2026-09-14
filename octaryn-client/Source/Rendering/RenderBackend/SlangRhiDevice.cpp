#include "SlangRhiDevice.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-gfx.h>
#endif

namespace octaryn::client::rendering {

SlangRhiDeviceProbeResult probe_slang_rhi_device() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  gfx::IDevice::Desc desc{};
  desc.deviceType = gfx::DeviceType::Vulkan;
  desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  gfx::IDevice *device = nullptr;
  const SlangResult result = gfx::gfxCreateDevice(&desc, &device);
  if (SLANG_SUCCEEDED(result) && device != nullptr) {
    device->release();
    return SlangRhiDeviceProbeResult{true, true, "created"};
  }
  return SlangRhiDeviceProbeResult{true, false, "create_failed"};
#else
  return SlangRhiDeviceProbeResult{false, false, "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
