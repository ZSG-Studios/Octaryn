#include "SlangRhiFrameResources.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-gfx.h>
#include <slang-com-ptr.h>
#endif

#include <cstdint>
#include <cstring>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

constexpr std::uint32_t ProbeUploadWords[] = {0x4f435441u, 0x52594e21u};

bool result_succeeded(SlangResult result) { return SLANG_SUCCEEDED(result); }

} // namespace
#endif

SlangRhiFrameResourceProbeResult probe_slang_rhi_frame_resources() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  SlangRhiFrameResourceProbeResult probe{
      true, false, false, false, false, false, false, false, false, false,
      false, false, false, false, false, false, "not_started"};

  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  Slang::ComPtr<gfx::IDevice> device;
  if (!result_succeeded(gfx::gfxCreateDevice(&device_desc, device.writeRef())) ||
      device == nullptr) {
    probe.status = "device_create_failed";
    return probe;
  }
  probe.device_created = true;

  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  if (!result_succeeded(device->createCommandQueue(queue_desc, queue.writeRef())) ||
      queue == nullptr) {
    probe.status = "queue_create_failed";
    return probe;
  }
  probe.queue_created = true;

  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.flags = gfx::ITransientResourceHeap::Flags::None;
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 4;
  heap_desc.uavDescriptorCount = 4;
  heap_desc.constantBufferDescriptorCount = 4;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  if (!result_succeeded(device->createTransientResourceHeap(heap_desc,
                                                           heap.writeRef())) ||
      heap == nullptr) {
    probe.status = "transient_heap_create_failed";
    return probe;
  }
  probe.transient_heap_created = true;

  if (!result_succeeded(heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return probe;
  }

  gfx::IBufferResource::Desc buffer_desc{};
  buffer_desc.type = gfx::IResource::Type::Buffer;
  buffer_desc.defaultState = gfx::ResourceState::CopyDestination;
  buffer_desc.allowedStates = gfx::ResourceStateSet(
      gfx::ResourceState::CopyDestination, gfx::ResourceState::CopySource);
  buffer_desc.memoryType = gfx::MemoryType::DeviceLocal;
  buffer_desc.sizeInBytes = sizeof(ProbeUploadWords);
  buffer_desc.elementSize = sizeof(std::uint32_t);
  Slang::ComPtr<gfx::IBufferResource> buffer;
  if (!result_succeeded(
          device->createBufferResource(buffer_desc, nullptr, buffer.writeRef())) ||
      buffer == nullptr) {
    probe.status = "buffer_create_failed";
    return probe;
  }
  probe.buffer_created = true;

  constexpr gfx::Format color_format = gfx::Format::R8G8B8A8_UNORM;
  gfx::ClearValue clear_value{};
  clear_value.color.floatValues[0] = 0.05f;
  clear_value.color.floatValues[1] = 0.10f;
  clear_value.color.floatValues[2] = 0.15f;
  clear_value.color.floatValues[3] = 1.0f;

  gfx::ITextureResource::Desc color_desc{};
  color_desc.type = gfx::IResource::Type::Texture2D;
  color_desc.defaultState = gfx::ResourceState::RenderTarget;
  color_desc.allowedStates = gfx::ResourceStateSet(
      gfx::ResourceState::RenderTarget, gfx::ResourceState::CopySource);
  color_desc.memoryType = gfx::MemoryType::DeviceLocal;
  color_desc.size.width = 16;
  color_desc.size.height = 16;
  color_desc.size.depth = 1;
  color_desc.arraySize = 1;
  color_desc.numMipLevels = 1;
  color_desc.format = color_format;
  color_desc.sampleDesc.numSamples = 1;
  color_desc.optimalClearValue = &clear_value;
  Slang::ComPtr<gfx::ITextureResource> color_target;
  if (!result_succeeded(device->createTextureResource(
          color_desc, nullptr, color_target.writeRef())) ||
      color_target == nullptr) {
    probe.status = "offscreen_target_create_failed";
    return probe;
  }

  gfx::IResourceView::Desc target_view_desc{};
  target_view_desc.type = gfx::IResourceView::Type::RenderTarget;
  target_view_desc.format = color_format;
  target_view_desc.renderTarget.shape = gfx::IResource::Type::Texture2D;
  target_view_desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
  target_view_desc.subresourceRange.mipLevel = 0;
  target_view_desc.subresourceRange.mipLevelCount = 1;
  target_view_desc.subresourceRange.baseArrayLayer = 0;
  target_view_desc.subresourceRange.layerCount = 1;
  Slang::ComPtr<gfx::IResourceView> color_target_view;
  if (!result_succeeded(device->createTextureView(
          color_target, target_view_desc, color_target_view.writeRef())) ||
      color_target_view == nullptr) {
    probe.status = "offscreen_target_view_create_failed";
    return probe;
  }
  probe.offscreen_target_created = true;

  gfx::IFramebufferLayout::TargetLayout color_layout{};
  color_layout.format = color_format;
  color_layout.sampleCount = 1;
  gfx::IFramebufferLayout::Desc framebuffer_layout_desc{};
  framebuffer_layout_desc.renderTargetCount = 1;
  framebuffer_layout_desc.renderTargets = &color_layout;
  Slang::ComPtr<gfx::IFramebufferLayout> framebuffer_layout;
  if (!result_succeeded(device->createFramebufferLayout(
          framebuffer_layout_desc, framebuffer_layout.writeRef())) ||
      framebuffer_layout == nullptr) {
    probe.status = "framebuffer_layout_create_failed";
    return probe;
  }

  gfx::IResourceView *render_targets[] = {color_target_view};
  gfx::IFramebuffer::Desc framebuffer_desc{};
  framebuffer_desc.renderTargetCount = 1;
  framebuffer_desc.renderTargetViews = render_targets;
  framebuffer_desc.depthStencilView = nullptr;
  framebuffer_desc.layout = framebuffer_layout;
  Slang::ComPtr<gfx::IFramebuffer> framebuffer;
  if (!result_succeeded(
          device->createFramebuffer(framebuffer_desc, framebuffer.writeRef())) ||
      framebuffer == nullptr) {
    probe.status = "framebuffer_create_failed";
    return probe;
  }
  probe.framebuffer_created = true;

  gfx::IRenderPassLayout::TargetAccessDesc color_access{};
  color_access.loadOp = gfx::IRenderPassLayout::TargetLoadOp::Clear;
  color_access.stencilLoadOp = gfx::IRenderPassLayout::TargetLoadOp::DontCare;
  color_access.storeOp = gfx::IRenderPassLayout::TargetStoreOp::Store;
  color_access.stencilStoreOp =
      gfx::IRenderPassLayout::TargetStoreOp::DontCare;
  color_access.initialState = gfx::ResourceState::RenderTarget;
  color_access.finalState = gfx::ResourceState::RenderTarget;
  gfx::IRenderPassLayout::Desc render_pass_desc{};
  render_pass_desc.framebufferLayout = framebuffer_layout;
  render_pass_desc.renderTargetCount = 1;
  render_pass_desc.renderTargetAccess = &color_access;
  render_pass_desc.depthStencilAccess = nullptr;
  Slang::ComPtr<gfx::IRenderPassLayout> render_pass;
  if (!result_succeeded(device->createRenderPassLayout(
          render_pass_desc, render_pass.writeRef())) ||
      render_pass == nullptr) {
    probe.status = "render_pass_create_failed";
    return probe;
  }
  probe.render_pass_created = true;

  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!result_succeeded(heap->createCommandBuffer(command_buffer.writeRef())) ||
      command_buffer == nullptr) {
    probe.status = "command_buffer_create_failed";
    return probe;
  }
  probe.command_buffer_created = true;
  probe.frame_begun = true;

  gfx::IResourceCommandEncoder *encoder = nullptr;
  command_buffer->encodeResourceCommands(&encoder);
  if (encoder == nullptr) {
    probe.status = "resource_encoder_create_failed";
    return probe;
  }
  encoder->uploadBufferData(buffer, 0, sizeof(ProbeUploadWords),
                            const_cast<std::uint32_t *>(ProbeUploadWords));
  encoder->bufferBarrier(buffer, gfx::ResourceState::CopyDestination,
                         gfx::ResourceState::CopySource);
  encoder->endEncoding();
  encoder->release();
  probe.upload_encoded = true;

  gfx::IRenderCommandEncoder *render_encoder = nullptr;
  command_buffer->encodeRenderCommands(render_pass, framebuffer, &render_encoder);
  if (render_encoder == nullptr) {
    probe.status = "render_encoder_create_failed";
    return probe;
  }
  render_encoder->endEncoding();
  render_encoder->release();
  probe.frame_encoded = true;

  command_buffer->close();
  probe.frame_ended = true;

  gfx::IFence::Desc fence_desc{};
  fence_desc.initialValue = 0;
  Slang::ComPtr<gfx::IFence> fence;
  if (!result_succeeded(device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return probe;
  }

  queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;

  gfx::IFence *fences[] = {fence};
  std::uint64_t fence_values[] = {1};
  if (!result_succeeded(device->waitForFences(1, fences, fence_values, true,
                                             gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return probe;
  }
  probe.fence_completed = true;

  Slang::ComPtr<ISlangBlob> readback;
  if (!result_succeeded(device->readBufferResource(
          buffer, 0, sizeof(ProbeUploadWords), readback.writeRef())) ||
      readback == nullptr) {
    probe.status = "buffer_readback_failed";
    return probe;
  }

  probe.readback_valid =
      readback->getBufferSize() == sizeof(ProbeUploadWords) &&
      std::memcmp(readback->getBufferPointer(), ProbeUploadWords,
                  sizeof(ProbeUploadWords)) == 0;
  if (!probe.readback_valid) {
    probe.status = "buffer_readback_mismatch";
    return probe;
  }

  if (!result_succeeded(heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return probe;
  }

  probe.status = "offscreen_frame_resources_validated";
  return probe;
#else
  return SlangRhiFrameResourceProbeResult{
      false, false, false, false, false, false, false, false, false, false,
      false, false, false, false, false, false, "runtime_unavailable"};
#endif
}

} // namespace octaryn::client::rendering
