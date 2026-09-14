#pragma once

#include "SlangRhiVoxelRasterFrame.h"
#include "SlangRhiVoxelRasterResources.h"
#include "SlangRhiVoxelRasterStreamBatch.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>
#endif

#include <cstdint>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)

struct SlangRhiVoxelRasterFrameSession {
  SlangRhiVoxelRasterStreamBatch batch;
  int chunk_count;
  Slang::ComPtr<gfx::IDevice> device;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  Slang::ComPtr<gfx::IPipelineState> face_pipe;
  Slang::ComPtr<gfx::IPipelineState> greedy_pipe;
  Slang::ComPtr<gfx::IPipelineState> prefix_pipe;
  Slang::ComPtr<gfx::IPipelineState> emit_pipe;
  Slang::ComPtr<gfx::IPipelineState> indirect_pipe;
  Slang::ComPtr<gfx::IPipelineState> raster_pipe;
  detail::SlangRhiVoxelRasterResources resources;
  Slang::ComPtr<gfx::ITextureResource> color_target;
  Slang::ComPtr<gfx::IResourceView> color_target_view;
  Slang::ComPtr<gfx::IFramebufferLayout> framebuffer_layout;
  Slang::ComPtr<gfx::IFramebuffer> framebuffer;
  Slang::ComPtr<gfx::IRenderPassLayout> render_pass;
  std::uint32_t frames_rendered;
};

bool create_frame_session(SlangRhiVoxelRasterFrameSession &session,
                          SlangRhiVoxelRasterFrameProbeResult &probe);

#endif

} // namespace octaryn::client::rendering
