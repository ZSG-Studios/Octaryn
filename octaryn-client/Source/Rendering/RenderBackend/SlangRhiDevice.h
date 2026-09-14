#pragma once

namespace octaryn::client::rendering {

struct SlangRhiDeviceProbeResult {
  bool runtime_available;
  bool device_created;
  const char *status;
};

SlangRhiDeviceProbeResult probe_slang_rhi_device();

} // namespace octaryn::client::rendering
