#include "SlangRhiVoxelRasterFrame.h"

#include "SlangRhiVoxelRasterFrameGpu.h"
#include "SlangRhiVoxelRasterFrameSessionInternal.h"
#include "SlangRhiVoxelRasterPassGraph.h"
#include "SlangRhiVoxelRasterStreamBatch.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

using namespace octaryn::client::rendering::detail;
using octaryn::client::voxel::ColumnCoord;

std::uint64_t elapsed_microseconds(std::chrono::steady_clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start)
          .count());
}

bool encode_stream_update_and_reset(
    gfx::ICommandBuffer *command_buffer, SlangRhiVoxelRasterFrameSession &session,
    const SlangRhiVoxelRasterStreamBatch *updated_batch,
    SlangRhiVoxelRasterFrameProbeResult &probe) {
  if (updated_batch == nullptr && session.frames_rendered == 0u) {
    return true;
  }
  gfx::IResourceCommandEncoder *resource_encoder = nullptr;
  command_buffer->encodeResourceCommands(&resource_encoder);
  if (resource_encoder == nullptr) {
    probe.status = "resource_encoder_create_failed";
    return false;
  }
  if (updated_batch != nullptr) {
    if (updated_batch->headers.size() != session.batch.headers.size() ||
        updated_batch->palette_entries.size() !=
            session.batch.palette_entries.size() ||
        updated_batch->payload_bytes.size() != session.batch.payload_bytes.size() ||
        updated_batch->column_coords.size() != session.batch.column_coords.size() ||
        !upload_slang_rhi_voxel_raster_input_resources(resource_encoder,
                                                       *updated_batch,
                                                       session.resources)) {
      resource_encoder->endEncoding();
      resource_encoder->release();
      probe.status = "raster_stream_update_failed";
      return false;
    }
    session.batch = *updated_batch;
    probe.upload_staging_bytes = session.resources.upload_staging_bytes;
    ++probe.stream_updates;
  }
  if (session.frames_rendered > 0u) {
    resource_encoder->bufferBarrier(session.resources.masks,
                                    gfx::ResourceState::General,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.quad_counts,
                                    gfx::ResourceState::General,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.offsets,
                                    gfx::ResourceState::General,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.quads,
                                    gfx::ResourceState::General,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.emit_counts,
                                    gfx::ResourceState::General,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.commands,
                                    gfx::ResourceState::IndirectArgument,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.draw_count,
                                    gfx::ResourceState::IndirectArgument,
                                    gfx::ResourceState::UnorderedAccess);
    resource_encoder->bufferBarrier(session.resources.instances,
                                    gfx::ResourceState::CopySource,
                                    gfx::ResourceState::UnorderedAccess);
  }
  resource_encoder->endEncoding();
  resource_encoder->release();
  return true;
}

bool encode_pass_graph(gfx::ICommandBuffer *command_buffer,
                       SlangRhiVoxelRasterFrameSession &session,
                       SlangRhiVoxelRasterFrameProbeResult &probe) {
  gfx::IComputeCommandEncoder *compute = nullptr;
  command_buffer->encodeComputeCommands(&compute);
  if (compute == nullptr) {
    probe.status = "compute_encoder_create_failed";
    return false;
  }
  const SlangRhiVoxelRasterPassGraphPipelines pipelines{
      session.face_pipe, session.greedy_pipe, session.prefix_pipe,
      session.emit_pipe, session.indirect_pipe};
  const SlangRhiVoxelRasterPassGraphViews pass_views =
      slang_rhi_voxel_raster_pass_graph_views(session.resources);
  const SlangRhiVoxelRasterPassGraphBuffers pass_buffers =
      slang_rhi_voxel_raster_pass_graph_buffers(session.resources);
  if (dispatch_slang_rhi_voxel_raster_pass_graph(
          compute, pipelines, pass_views, pass_buffers,
          session.chunk_count) != nullptr) {
    compute->endEncoding();
    compute->release();
    probe.status = "compute_dispatch_failed";
    return false;
  }
  compute->endEncoding();
  compute->release();
  probe.compute_dispatched = true;
  return true;
}

bool encode_indirect_draw(gfx::ICommandBuffer *command_buffer,
                          SlangRhiVoxelRasterFrameSession &session,
                          SlangRhiVoxelRasterFrameProbeResult &probe) {
  gfx::IResourceCommandEncoder *resource_encoder = nullptr;
  command_buffer->encodeResourceCommands(&resource_encoder);
  if (resource_encoder == nullptr) {
    probe.status = "resource_encoder_create_failed";
    return false;
  }
  resource_encoder->bufferBarrier(session.resources.commands,
                                  gfx::ResourceState::UnorderedAccess,
                                  gfx::ResourceState::IndirectArgument);
  resource_encoder->bufferBarrier(session.resources.draw_count,
                                  gfx::ResourceState::UnorderedAccess,
                                  gfx::ResourceState::IndirectArgument);
  resource_encoder->bufferBarrier(session.resources.instances,
                                  gfx::ResourceState::UnorderedAccess,
                                  gfx::ResourceState::CopySource);
  if (session.frames_rendered > 0u) {
    resource_encoder->textureBarrier(session.color_target,
                                     gfx::ResourceState::CopySource,
                                     gfx::ResourceState::RenderTarget);
  }
  resource_encoder->endEncoding();
  resource_encoder->release();

  gfx::IRenderCommandEncoder *render = nullptr;
  command_buffer->encodeRenderCommands(session.render_pass, session.framebuffer,
                                       &render);
  if (render == nullptr) {
    probe.status = "render_encoder_create_failed";
    return false;
  }
  gfx::Viewport viewport{};
  viewport.extentX = static_cast<float>(RasterWidth);
  viewport.extentY = static_cast<float>(RasterHeight);
  viewport.maxZ = 1.0f;
  render->setViewportAndScissor(viewport);
  render->setPrimitiveTopology(gfx::PrimitiveTopology::TriangleList);
  render->setIndexBuffer(session.resources.index_buffer, gfx::Format::R16_UINT);
  if (!bind_raster(render, session.raster_pipe,
                   session.resources.quad_srv_view) ||
      !ok(render->drawIndexedIndirect(session.chunk_count,
                                      session.resources.commands, 0,
                                      session.resources.draw_count, 0))) {
    render->endEncoding();
    render->release();
    probe.status = "indirect_draw_failed";
    return false;
  }
  render->endEncoding();
  render->release();
  resource_encoder = nullptr;
  command_buffer->encodeResourceCommands(&resource_encoder);
  if (resource_encoder == nullptr) {
    probe.status = "readback_transition_encoder_failed";
    return false;
  }
  resource_encoder->textureBarrier(session.color_target,
                                   gfx::ResourceState::RenderTarget,
                                   gfx::ResourceState::CopySource);
  resource_encoder->endEncoding();
  resource_encoder->release();
  probe.indirect_draw_encoded = true;
  return true;
}

bool submit_and_wait(gfx::ICommandBuffer *command_buffer,
                     SlangRhiVoxelRasterFrameSession &session,
                     SlangRhiVoxelRasterFrameProbeResult &probe) {
  command_buffer->close();
  gfx::IFence::Desc fence_desc{};
  Slang::ComPtr<gfx::IFence> fence;
  if (!ok(session.device->createFence(fence_desc, fence.writeRef())) ||
      fence == nullptr) {
    probe.status = "fence_create_failed";
    return false;
  }
  session.queue->executeCommandBuffer(command_buffer, fence, 1);
  probe.submitted = true;
  gfx::IFence *fences[] = {fence};
  std::uint64_t values[] = {1};
  if (!ok(session.device->waitForFences(1, fences, values, true,
                                        gfx::kTimeoutInfinite))) {
    probe.status = "fence_wait_failed";
    return false;
  }
  probe.fence_completed = true;
  return true;
}

bool readback_frame(SlangRhiVoxelRasterFrameSession &session,
                    SlangRhiVoxelRasterFrameProbeResult &probe) {
  std::vector<std::uint32_t> draw_counts(1u), instance_counts(1u);
  Slang::ComPtr<ISlangBlob> draw_blob, instance_blob, color_blob;
  gfx::Size row_pitch = 0;
  gfx::Size pixel_size = 0;
  if (!ok(session.device->readBufferResource(session.resources.draw_count, 0,
                                            sizeof(std::uint32_t),
                                            draw_blob.writeRef())) ||
      !ok(session.device->readBufferResource(session.resources.instances, 0,
                                            sizeof(std::uint32_t),
                                            instance_blob.writeRef())) ||
      !ok(session.device->readTextureResource(
          session.color_target, gfx::ResourceState::CopySource,
          color_blob.writeRef(), &row_pitch, &pixel_size)) ||
      draw_blob == nullptr || instance_blob == nullptr || color_blob == nullptr) {
    probe.status = "readback_failed";
    return false;
  }
  std::memcpy(draw_counts.data(), draw_blob->getBufferPointer(),
              sizeof(std::uint32_t));
  std::memcpy(instance_counts.data(), instance_blob->getBufferPointer(),
              sizeof(std::uint32_t));
  probe.draw_count = draw_counts[0];
  probe.total_instances = instance_counts[0];
  probe.non_clear_pixels =
      count_non_clear_pixels(color_blob, row_pitch, pixel_size);
  probe.color_readback_valid =
      probe.draw_count > 0u && probe.total_instances > 0u &&
      probe.non_clear_pixels > 0u;
  if (!probe.color_readback_valid) {
    probe.status = "raster_readback_mismatch";
    return false;
  }
  probe.bounded_columns = session.batch.bounded_columns;
  probe.live_columns = session.batch.available_columns;
  probe.retained_gpu_bytes = session.resources.retained_gpu_bytes;
  probe.upload_staging_bytes = session.resources.upload_staging_bytes;
  probe.stream_center_x = session.batch.stream_center.x;
  probe.stream_center_z = session.batch.stream_center.z;
  probe.first_column_x = session.batch.column_coords.front().x;
  probe.first_column_z = session.batch.column_coords.front().z;
  probe.live_stream_source = session.batch.live_stream_source;
  return true;
}

bool render_session_frame(
    SlangRhiVoxelRasterFrameSession &session,
    const SlangRhiVoxelRasterStreamBatch *updated_batch,
    SlangRhiVoxelRasterFrameProbeResult &probe,
    SlangRhiVoxelRasterFrameTiming &timing) {
  const auto frame_started = std::chrono::steady_clock::now();
  if (session.frames_rendered > 0u && !ok(session.heap->synchronizeAndReset())) {
    probe.status = "transient_heap_reset_failed";
    return false;
  }
  Slang::ComPtr<gfx::ICommandBuffer> command_buffer;
  if (!ok(session.heap->createCommandBuffer(command_buffer.writeRef())) ||
      command_buffer == nullptr) {
    probe.status = "command_buffer_create_failed";
    return false;
  }
  probe.command_buffer_created = true;
  auto stage_started = std::chrono::steady_clock::now();
  if (!encode_stream_update_and_reset(command_buffer, session, updated_batch,
                                      probe)) {
    return false;
  }
  timing.stream_upload_microseconds = elapsed_microseconds(stage_started);
  stage_started = std::chrono::steady_clock::now();
  if (!encode_pass_graph(command_buffer, session, probe)) { return false; }
  timing.pass_graph_microseconds = elapsed_microseconds(stage_started);
  stage_started = std::chrono::steady_clock::now();
  if (!encode_indirect_draw(command_buffer, session, probe)) { return false; }
  timing.draw_microseconds = elapsed_microseconds(stage_started);
  stage_started = std::chrono::steady_clock::now();
  if (!submit_and_wait(command_buffer, session, probe) ||
      !readback_frame(session, probe)) {
    return false;
  }
  timing.submit_readback_microseconds = elapsed_microseconds(stage_started);
  if (!ok(session.heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return false;
  }
  ++session.frames_rendered;
  probe.frames_rendered = session.frames_rendered;
  timing.frame_microseconds = elapsed_microseconds(frame_started);
  return true;
}

} // namespace
#endif

SlangRhiVoxelRasterFrameSession *
create_slang_rhi_voxel_raster_frame_session(
    SlangRhiVoxelRasterFrameProbeResult &probe) {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  probe = raster_result("not_started");
  auto *session = new SlangRhiVoxelRasterFrameSession{};
  if (!create_frame_session(*session, probe)) {
    delete session;
    return nullptr;
  }
  return session;
#else
  probe = {};
  probe.status = "runtime_unavailable";
  return nullptr;
#endif
}

void destroy_slang_rhi_voxel_raster_frame_session(
    SlangRhiVoxelRasterFrameSession *session) {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  delete session;
#else
  (void)session;
#endif
}

SlangRhiVoxelRasterFrameRenderResult
render_slang_rhi_voxel_raster_session_frame(
    SlangRhiVoxelRasterFrameSession *session,
    const SlangRhiVoxelRasterStreamBatch *updated_batch,
    SlangRhiVoxelRasterFrameProbeResult &probe) {
  SlangRhiVoxelRasterFrameRenderResult result{probe, {}};
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  if (session == nullptr) {
    result.probe.status = "frame_session_missing";
    return result;
  }
  if (!render_session_frame(*session, updated_batch, result.probe,
                            result.timing)) {
    probe = result.probe;
    return result;
  }
  result.probe.status = "gpu_voxel_raster_frame_validated";
  probe = result.probe;
  return result;
#else
  (void)session;
  (void)updated_batch;
  result.probe = {};
  result.probe.status = "runtime_unavailable";
  probe = result.probe;
  return result;
#endif
}

SlangRhiVoxelRasterFrameProbeResult render_slang_rhi_voxel_raster_frame() {
#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  SlangRhiVoxelRasterFrameProbeResult probe{};
  SlangRhiVoxelRasterFrameSession *session =
      create_slang_rhi_voxel_raster_frame_session(probe);
  if (session == nullptr) {
    return probe;
  }
  auto first_frame =
      render_slang_rhi_voxel_raster_session_frame(session, nullptr, probe);
  if (std::strcmp(first_frame.probe.status,
                  "gpu_voxel_raster_frame_validated") != 0) {
    destroy_slang_rhi_voxel_raster_frame_session(session);
    return probe;
  }
  const SlangRhiVoxelRasterStreamBatch moved_batch =
      build_slang_rhi_voxel_raster_stream_batch_for_center(ColumnCoord{1, 0});
  auto second_frame = render_slang_rhi_voxel_raster_session_frame(
      session, &moved_batch, probe);
  destroy_slang_rhi_voxel_raster_frame_session(session);
  if (std::strcmp(second_frame.probe.status,
                  "gpu_voxel_raster_frame_validated") != 0) {
    return probe;
  }
  probe.session_reused = probe.frames_rendered >= 2u;
  probe.status = "gpu_voxel_raster_frame_validated";
  return probe;
#else
  SlangRhiVoxelRasterFrameProbeResult probe{};
  probe.status = "runtime_unavailable";
  return probe;
#endif
}

SlangRhiVoxelRasterFrameProbeResult probe_slang_rhi_voxel_raster_frame() {
  return render_slang_rhi_voxel_raster_frame();
}

} // namespace octaryn::client::rendering
