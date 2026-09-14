#include "SlangRhiVoxelRasterFrameSessionInternal.h"

#include "SlangRhiVoxelRasterFrameGpu.h"

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

using namespace octaryn::client::rendering::detail;

bool create_device_queue_heap(SlangRhiVoxelRasterFrameSession &session,
                              SlangRhiVoxelRasterFrameProbeResult &probe) {
  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  device_desc.slang.targetProfile = "spirv_1_3";
  if (!ok(gfx::gfxCreateDevice(&device_desc, session.device.writeRef())) ||
      session.device == nullptr) {
    probe.status = "device_create_failed";
    return false;
  }
  probe.device_created = true;

  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  if (!ok(session.device->createCommandQueue(queue_desc,
                                             session.queue.writeRef())) ||
      session.queue == nullptr) {
    probe.status = "queue_create_failed";
    return false;
  }
  probe.queue_created = true;

  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.flags = gfx::ITransientResourceHeap::Flags::None;
  heap_desc.constantBufferSize = 4096;
  heap_desc.srvDescriptorCount = 24;
  heap_desc.uavDescriptorCount = 24;
  heap_desc.constantBufferDescriptorCount = 4;
  if (!ok(session.device->createTransientResourceHeap(
          heap_desc, session.heap.writeRef())) ||
      session.heap == nullptr || !ok(session.heap->synchronizeAndReset())) {
    probe.status = "transient_heap_create_failed";
    return false;
  }
  probe.transient_heap_created = true;
  return true;
}

bool create_pipelines(SlangRhiVoxelRasterFrameSession &session,
                      SlangRhiVoxelRasterFrameProbeResult &probe) {
  if (!create_slang_compute_pipeline(session.device, FaceMaskShaderPath,
                               session.face_pipe) ||
      !create_slang_compute_pipeline(session.device, GreedyCountShaderPath,
                               session.greedy_pipe) ||
      !create_slang_compute_pipeline(session.device, PrefixScanShaderPath,
                               session.prefix_pipe) ||
      !create_slang_compute_pipeline(session.device, PackedEmitShaderPath,
                               session.emit_pipe) ||
      !create_slang_compute_pipeline(session.device, IndirectShaderPath,
                               session.indirect_pipe)) {
    probe.status = "compute_pipeline_create_failed";
    return false;
  }
  probe.compute_pipelines_created = true;
  return true;
}

bool create_render_target(SlangRhiVoxelRasterFrameSession &session,
                          SlangRhiVoxelRasterFrameProbeResult &probe) {
  constexpr gfx::Format color_format = gfx::Format::R8G8B8A8_UNORM;
  gfx::ClearValue clear_value{};
  clear_value.color.floatValues[3] = 1.0f;
  gfx::ITextureResource::Desc color_desc{};
  color_desc.type = gfx::IResource::Type::Texture2D;
  color_desc.defaultState = gfx::ResourceState::RenderTarget;
  color_desc.allowedStates = gfx::ResourceStateSet(
      gfx::ResourceState::RenderTarget, gfx::ResourceState::CopySource);
  color_desc.memoryType = gfx::MemoryType::DeviceLocal;
  color_desc.size = {RasterWidth, RasterHeight, 1};
  color_desc.arraySize = 1;
  color_desc.numMipLevels = 1;
  color_desc.format = color_format;
  color_desc.sampleDesc.numSamples = 1;
  color_desc.optimalClearValue = &clear_value;
  if (!ok(session.device->createTextureResource(
          color_desc, nullptr, session.color_target.writeRef())) ||
      session.color_target == nullptr) {
    probe.status = "color_target_create_failed";
    return false;
  }
  probe.retained_gpu_bytes +=
      static_cast<std::uint64_t>(RasterWidth) * RasterHeight * 4u;

  gfx::IResourceView::Desc target_view_desc{};
  target_view_desc.type = gfx::IResourceView::Type::RenderTarget;
  target_view_desc.format = color_format;
  target_view_desc.renderTarget.shape = gfx::IResource::Type::Texture2D;
  target_view_desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
  target_view_desc.subresourceRange.mipLevelCount = 1;
  target_view_desc.subresourceRange.layerCount = 1;
  if (!ok(session.device->createTextureView(
          session.color_target, target_view_desc,
          session.color_target_view.writeRef())) ||
      session.color_target_view == nullptr) {
    probe.status = "color_target_view_create_failed";
    return false;
  }

  gfx::IFramebufferLayout::TargetLayout color_layout{color_format, 1};
  gfx::IFramebufferLayout::Desc framebuffer_layout_desc{};
  framebuffer_layout_desc.renderTargetCount = 1;
  framebuffer_layout_desc.renderTargets = &color_layout;
  if (!ok(session.device->createFramebufferLayout(
          framebuffer_layout_desc, session.framebuffer_layout.writeRef())) ||
      session.framebuffer_layout == nullptr ||
      !create_graphics_pipeline(session.device, session.framebuffer_layout,
                                session.raster_pipe)) {
    probe.status = "graphics_pipeline_create_failed";
    return false;
  }
  probe.graphics_pipeline_created = true;

  gfx::IResourceView *render_targets[] = {session.color_target_view};
  gfx::IFramebuffer::Desc framebuffer_desc{};
  framebuffer_desc.renderTargetCount = 1;
  framebuffer_desc.renderTargetViews = render_targets;
  framebuffer_desc.layout = session.framebuffer_layout;
  if (!ok(session.device->createFramebuffer(framebuffer_desc,
                                            session.framebuffer.writeRef())) ||
      session.framebuffer == nullptr) {
    probe.status = "framebuffer_create_failed";
    return false;
  }

  gfx::IRenderPassLayout::TargetAccessDesc color_access{};
  color_access.loadOp = gfx::IRenderPassLayout::TargetLoadOp::Clear;
  color_access.storeOp = gfx::IRenderPassLayout::TargetStoreOp::Store;
  color_access.initialState = gfx::ResourceState::RenderTarget;
  color_access.finalState = gfx::ResourceState::RenderTarget;
  gfx::IRenderPassLayout::Desc render_pass_desc{};
  render_pass_desc.framebufferLayout = session.framebuffer_layout;
  render_pass_desc.renderTargetCount = 1;
  render_pass_desc.renderTargetAccess = &color_access;
  if (!ok(session.device->createRenderPassLayout(
          render_pass_desc, session.render_pass.writeRef())) ||
      session.render_pass == nullptr) {
    probe.status = "render_pass_create_failed";
    return false;
  }
  return true;
}

} // namespace

bool create_frame_session(SlangRhiVoxelRasterFrameSession &session,
                          SlangRhiVoxelRasterFrameProbeResult &probe) {
  session.batch = build_slang_rhi_voxel_raster_stream_batch();
  session.chunk_count = static_cast<int>(session.batch.headers.size());
  if (session.chunk_count == 0 ||
      session.batch.bounded_columns != session.batch.headers.size() ||
      session.batch.column_coords.size() != session.batch.headers.size()) {
    probe.status = "raster_batch_invalid";
    return false;
  }
  probe.bounded_columns = session.batch.bounded_columns;
  probe.live_columns = session.batch.available_columns;
  probe.stream_center_x = session.batch.stream_center.x;
  probe.stream_center_z = session.batch.stream_center.z;
  probe.first_column_x = session.batch.column_coords.front().x;
  probe.first_column_z = session.batch.column_coords.front().z;

  if (!create_device_queue_heap(session, probe) ||
      !create_pipelines(session, probe)) {
    return false;
  }
  if (!create_slang_rhi_voxel_raster_resources(session.device, session.batch,
                                               session.resources)) {
    probe.status = "raster_resource_create_failed";
    return false;
  }
  probe.buffers_created = true;
  probe.resources_bound = true;
  probe.retained_frame_resources = true;
  probe.retained_gpu_bytes = session.resources.retained_gpu_bytes;
  probe.upload_staging_bytes = session.resources.upload_staging_bytes;
  return create_render_target(session, probe);
}

#endif

} // namespace octaryn::client::rendering
