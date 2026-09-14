#include "RenderBackend.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>

namespace octaryn::client::rendering {
RenderBackendStatus active_render_backend_status() {
  RenderBackendStatus status{};
  status.kind = RenderBackendKind::SlangRhi;
  status.slang_shaders_required = true;
  status.slang_device_status = "standalone_slang_rhi_device_failed";
  status.slang_frame_resource_status = "not_validated";
  status.slang_runtime_available = rhi::getRHI() != nullptr;
  if (!status.slang_runtime_available) return status;
  rhi::DeviceDesc description{};
  description.deviceType = rhi::DeviceType::Vulkan;
  Slang::ComPtr<rhi::IDevice> device;
  if (SLANG_FAILED(rhi::getRHI()->createDevice(description, device.writeRef()))) return status;
  status.slang_device_created = true;
  status.slang_device_status = "standalone_slang_rhi_vulkan";
  Slang::ComPtr<rhi::ICommandQueue> queue;
  if (SLANG_FAILED(device->getQueue(rhi::QueueType::Graphics, queue.writeRef()))) return status;
  const std::array<uint32_t, 4> initial{1, 2, 3, 4};
  rhi::BufferDesc buffer_desc{};
  buffer_desc.size = sizeof(initial);
  buffer_desc.usage = rhi::BufferUsage::CopySource | rhi::BufferUsage::CopyDestination;
  buffer_desc.defaultState = rhi::ResourceState::CopyDestination;
  auto buffer = device->createBuffer(buffer_desc, initial.data());
  if (!buffer) return status;
  auto encoder = queue->createCommandEncoder();
  if (!encoder) return status;
  status.slang_frame_begun = true;
  encoder->clearBuffer(buffer);
  status.slang_frame_encoded = true;
  auto commands = encoder->finish();
  if (!commands) return status;
  status.slang_frame_ended = true;
  if (SLANG_FAILED(queue->submit(commands))) return status;
  status.slang_frame_submitted = true;
  if (SLANG_FAILED(queue->waitOnHost())) return status;
  std::array<uint32_t, 4> actual{1, 1, 1, 1};
  if (SLANG_FAILED(device->readBuffer(buffer, 0, sizeof(actual), actual.data()))) return status;
  status.slang_frame_resources_validated = actual == std::array<uint32_t, 4>{};
  status.slang_frame_lifecycle_validated = status.slang_frame_resources_validated;
  status.slang_frame_resource_status = status.slang_frame_resources_validated
      ? "gpu_clear_submit_readback_validated" : "gpu_readback_mismatch";
  return status;
}
const char* render_backend_name(RenderBackendKind) { return "SlangRhi"; }
} // namespace octaryn::client::rendering
