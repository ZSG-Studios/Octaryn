#pragma once

#include "SlangComputePipeline.h"
#include "SlangShaderPath.h"
#include "SlangRhiVoxelRasterFrame.h"
#include "VoxelLayout.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace octaryn::client::rendering::detail {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)

constexpr const char *FaceMaskShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelFaceMaskProbe.slang";
constexpr const char *GreedyCountShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelGreedyCountProbe.slang";
constexpr const char *PrefixScanShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelPrefixScanProbe.slang";
constexpr const char *PackedEmitShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelPackedQuadEmitProbe.slang";
constexpr const char *IndirectShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelIndirectGenerationProbe.slang";
constexpr const char *RasterShaderPath =
    "octaryn-client/Shaders/Voxel/VoxelRasterProbe.slang";
constexpr std::uint32_t ExpectedTotalQuads = 98440u;
constexpr std::size_t FaceMaskValuesPerChunk =
    static_cast<std::size_t>(octaryn::client::voxel::ChunkVoxelCount);
constexpr std::uint32_t RasterWidth = 64u;
constexpr std::uint32_t RasterHeight = 64u;
constexpr std::uint8_t ClearR = 0u;
constexpr std::uint8_t ClearG = 0u;
constexpr std::uint8_t ClearB = 0u;
constexpr std::uint8_t ClearA = 255u;

inline bool ok(SlangResult result) { return SLANG_SUCCEEDED(result); }

inline void append_bytes(std::vector<std::uint8_t> &bytes, const void *source,
                         std::size_t byte_count) {
  if (byte_count == 0u) { return; }
  const auto *typed = static_cast<const std::uint8_t *>(source);
  bytes.insert(bytes.end(), typed, typed + byte_count);
}

template <typename T>
std::vector<std::uint8_t> serialize_values(const std::vector<T> &values) {
  std::vector<std::uint8_t> bytes;
  append_bytes(bytes, values.data(), values.size() * sizeof(T));
  return bytes;
}

inline bool create_buffer(gfx::IDevice *device, std::size_t byte_count,
                          std::size_t element_size,
                          gfx::ResourceState state,
                          gfx::ResourceStateSet states,
                          const void *initial_data,
                          Slang::ComPtr<gfx::IBufferResource> &buffer) {
  gfx::IBufferResource::Desc desc{};
  desc.type = gfx::IResource::Type::Buffer;
  desc.defaultState = state;
  desc.allowedStates = states;
  desc.memoryType = gfx::MemoryType::DeviceLocal;
  desc.sizeInBytes = byte_count == 0u ? element_size : byte_count;
  desc.elementSize = element_size;
  const void *upload_data = byte_count == 0u ? nullptr : initial_data;
  return ok(device->createBufferResource(desc, upload_data,
                                         buffer.writeRef())) &&
         buffer != nullptr;
}

inline bool create_view(gfx::IDevice *device, gfx::IBufferResource *buffer,
                        gfx::IResourceView::Type type,
                        std::size_t byte_count,
                        Slang::ComPtr<gfx::IResourceView> &view) {
  gfx::IResourceView::Desc desc{};
  desc.type = type;
  desc.format = gfx::Format::Unknown;
  desc.bufferRange.offset = 0;
  desc.bufferRange.size = byte_count == 0u ? 1u : byte_count;
  return ok(device->createBufferView(buffer, nullptr, desc, view.writeRef())) &&
         view != nullptr;
}



inline bool create_graphics_pipeline(
    gfx::IDevice *device, gfx::IFramebufferLayout *framebuffer_layout,
    Slang::ComPtr<gfx::IPipelineState> &pipeline) {
  gfx::IShaderProgram::CreateDesc2 program_desc{};
  program_desc.sourceType = gfx::ShaderModuleSourceType::SlangSourceFile;
  const auto shader_path = resolve_slang_shader_path(RasterShaderPath);
  if (shader_path.empty()) { return false; }
  program_desc.sourceData = const_cast<char *>(shader_path.c_str());
  program_desc.sourceDataSize = shader_path.size();
  const char *entry_points[] = {"vertex_main", "fragment_main"};
  program_desc.entryPointCount = 2;
  program_desc.entryPointNames = entry_points;
  Slang::ComPtr<gfx::IShaderProgram> program;
  Slang::ComPtr<ISlangBlob> diagnostics;
  if (!ok(device->createProgram2(program_desc, program.writeRef(),
                                 diagnostics.writeRef())) ||
      program == nullptr) {
    return false;
  }
  gfx::GraphicsPipelineStateDesc desc{};
  desc.program = program;
  desc.framebufferLayout = framebuffer_layout;
  desc.primitiveType = gfx::PrimitiveType::Triangle;
  desc.blend.targetCount = 1;
  return ok(device->createGraphicsPipelineState(desc, pipeline.writeRef())) &&
         pipeline != nullptr;
}

inline bool set_resource(gfx::IShaderObject *object, gfx::GfxIndex index,
                         gfx::IResourceView *view) {
  return object != nullptr && ok(object->setResource({0, index, 0}, view));
}

template <std::size_t N>
bool bind_compute(gfx::IComputeCommandEncoder *encoder,
                  gfx::IPipelineState *pipeline,
                  const std::array<gfx::IResourceView *, N> &views) {
  Slang::ComPtr<gfx::IShaderObject> root;
  if (!ok(encoder->bindPipeline(pipeline, root.writeRef())) || root == nullptr) {
    return false;
  }
  Slang::ComPtr<gfx::IShaderObject> entry;
  root->getEntryPoint(0, entry.writeRef());
  bool root_ok = true;
  bool entry_ok = true;
  for (std::size_t i = 0; i < views.size(); ++i) {
    const auto index = static_cast<gfx::GfxIndex>(i);
    root_ok &= set_resource(root, index, views[i]);
    entry_ok &= set_resource(entry, index, views[i]);
  }
  return root_ok || entry_ok;
}

inline bool bind_raster(gfx::IRenderCommandEncoder *encoder,
                        gfx::IPipelineState *pipeline,
                        gfx::IResourceView *quad_view) {
  Slang::ComPtr<gfx::IShaderObject> root;
  if (!ok(encoder->bindPipeline(pipeline, root.writeRef())) || root == nullptr) {
    return false;
  }
  Slang::ComPtr<gfx::IShaderObject> vertex;
  root->getEntryPoint(0, vertex.writeRef());
  return set_resource(root, 0, quad_view) || set_resource(vertex, 0, quad_view);
}

inline SlangRhiVoxelRasterFrameProbeResult raster_result(const char *status) {
  SlangRhiVoxelRasterFrameProbeResult result{};
  result.runtime_available = true;
  result.status = status;
  return result;
}

inline unsigned count_non_clear_pixels(ISlangBlob *blob, gfx::Size row_pitch,
                                       gfx::Size pixel_size) {
  const auto *pixels = static_cast<const std::uint8_t *>(blob->getBufferPointer());
  const auto row = static_cast<std::size_t>(row_pitch);
  const auto px = static_cast<std::size_t>(pixel_size);
  unsigned count = 0u;
  for (std::uint32_t y = 0; y < RasterHeight; ++y) {
    for (std::uint32_t x = 0; x < RasterWidth; ++x) {
      const auto *p = pixels + y * row + x * px;
      if (px >= 4u &&
          (p[0] != ClearR || p[1] != ClearG || p[2] != ClearB || p[3] != ClearA)) {
        ++count;
      }
    }
  }
  return count;
}

inline SlangRhiVoxelRasterFrameProbeResult
fail_compute_dispatch(gfx::IComputeCommandEncoder *encoder,
                      SlangRhiVoxelRasterFrameProbeResult probe) {
  encoder->endEncoding();
  encoder->release();
  probe.status = "compute_dispatch_failed";
  return probe;
}

#endif

} // namespace octaryn::client::rendering::detail
