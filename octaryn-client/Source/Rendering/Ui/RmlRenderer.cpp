#include "RmlRenderer.h"
#include "RhiShader.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/RenderInterface.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace octaryn::client::rendering {
namespace {
constexpr bool report_rml_frame(uint64_t frame, bool validation) {
  return frame == 1 || (validation && (frame == 301 || frame == 601));
}
static_assert([] {
  unsigned normal_reports = 0, validation_reports = 0;
  for (uint64_t frame = 0; frame <= 1201; ++frame) {
    normal_reports += report_rml_frame(frame, false);
    validation_reports += report_rml_frame(frame, true);
  }
  return normal_reports == 1 && validation_reports == 3 &&
      report_rml_frame(1, false) && report_rml_frame(301, true) && report_rml_frame(601, true) &&
      !report_rml_frame(std::numeric_limits<uint64_t>::max(), true);
}(), "RmlUi reports must stop after the initial retention samples");

struct Geometry {
  Slang::ComPtr<rhi::IBuffer> vertices, indices;
  uint32_t count{};
};
struct Texture {
  Slang::ComPtr<rhi::ITexture> image;
  Slang::ComPtr<rhi::ITextureView> view;
};
struct Draw {
  std::shared_ptr<Geometry> geometry;
  std::shared_ptr<Texture> texture;
  std::array<float, 20> uniforms{};
  Rml::Rectanglei clip;
  bool clipped{};
};
}

struct RmlRenderer final : Rml::RenderInterface {
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IRenderPipeline> pipeline;
  Slang::ComPtr<rhi::ISampler> sampler;
  std::unordered_map<Rml::CompiledGeometryHandle, std::shared_ptr<Geometry>> geometries;
  std::unordered_map<Rml::TextureHandle, std::shared_ptr<Texture>> textures;
  std::shared_ptr<Texture> white;
  std::vector<Draw> draws;
  Rml::Matrix4f transform{Rml::Matrix4f::Identity()};
  Rml::Rectanglei clip;
  Rml::CompiledGeometryHandle next_geometry{1};
  Rml::TextureHandle next_texture{1};
  uint64_t frames{}, compiled_count{};
  size_t geometry_peak{}, texture_peak{};
  bool clipped{}, failed{}, validation_diagnostics{};

  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                             Rml::Span<const int> indices) override {
    static_assert(sizeof(Rml::Vertex) == 20 && offsetof(Rml::Vertex, colour) == 8 &&
                  offsetof(Rml::Vertex, tex_coord) == 12 && sizeof(int) == 4);
    if (vertices.empty() || indices.empty() || indices.size() > std::numeric_limits<uint32_t>::max()) return 0;
    for (const int index : indices)
      if (index < 0 || static_cast<size_t>(index) >= vertices.size()) { failed = true; return 0; }
    auto geometry = std::make_shared<Geometry>();
    rhi::BufferDesc desc{};
    // Small immutable UI meshes use host-visible coherent storage. The pinned
    // RHI DeviceLocal initial-data path submits a transfer for each buffer; text
    // refreshes create many meshes. Upload copies before submission without a
    // transfer submission. Geometry stays immutable and draw/command refs retain it.
    desc.memoryType = rhi::MemoryType::Upload;
    desc.size = vertices.size() * sizeof(Rml::Vertex);
    desc.usage = rhi::BufferUsage::ShaderResource | rhi::BufferUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::ShaderResource;
    if (SLANG_FAILED(device->createBuffer(desc, vertices.data(), geometry->vertices.writeRef()))) {
      failed = true; return 0;
    }
    desc.size = indices.size() * sizeof(int);
    desc.elementSize = sizeof(int);
    desc.usage = rhi::BufferUsage::IndexBuffer | rhi::BufferUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::IndexBuffer;
    if (SLANG_FAILED(device->createBuffer(desc, indices.data(), geometry->indices.writeRef()))) {
      failed = true; return 0;
    }
    geometry->count = static_cast<uint32_t>(indices.size());
    const auto handle = next_geometry++;
    geometries.emplace(handle, std::move(geometry));
    ++compiled_count; geometry_peak = std::max(geometry_peak, geometries.size());
    return handle;
  }

  void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
                      Rml::TextureHandle texture) override {
    const auto geometry = geometries.find(handle);
    const auto image = textures.find(texture);
    if (geometry == geometries.end() || (texture && image == textures.end())) { failed = true; return; }
    Draw draw;
    draw.geometry = geometry->second;
    draw.texture = texture ? image->second : white;
    draw.clip = clip; draw.clipped = clipped;
    for (int row = 0; row < 4; ++row)
      for (int column = 0; column < 4; ++column)
        draw.uniforms[static_cast<size_t>(row * 4 + column)] = transform.GetRow(row)[column];
    draw.uniforms[16] = translation.x; draw.uniforms[17] = translation.y;
    draws.push_back(std::move(draw));
  }

  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override { geometries.erase(geometry); }

  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> bytes, Rml::Vector2i dimensions) override {
    if (dimensions.x <= 0 || dimensions.y <= 0 ||
        static_cast<uint64_t>(dimensions.x) * static_cast<uint64_t>(dimensions.y) * 4 != bytes.size()) {
      failed = true; return 0;
    }
    auto texture = std::make_shared<Texture>();
    rhi::TextureDesc desc{};
    desc.size = {static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y), 1};
    desc.format = rhi::Format::RGBA8Unorm;
    desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::ShaderResource;
    rhi::SubresourceData pixels{};
    pixels.data = bytes.data(); pixels.rowPitch = static_cast<rhi::Size>(dimensions.x) * 4;
    pixels.slicePitch = pixels.rowPitch * static_cast<rhi::Size>(dimensions.y);
    if (SLANG_FAILED(device->createTexture(desc, &pixels, texture->image.writeRef())) ||
        SLANG_FAILED(texture->image->getDefaultView(texture->view.writeRef()))) { failed = true; return 0; }
    const auto handle = next_texture++;
    textures.emplace(handle, std::move(texture));
    texture_peak = std::max(texture_peak, textures.size());
    return handle;
  }

  Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> loaded(SDL_LoadPNG(source.c_str()), SDL_DestroySurface);
    if (!loaded) {
      std::fprintf(stderr, "RmlUi image failed: %s: %s\n", source.c_str(), SDL_GetError());
      failed = true; return 0;
    }
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image(
        SDL_ConvertSurface(loaded.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!image || image->w <= 0 || image->h <= 0) { failed = true; return 0; }
    dimensions = {image->w, image->h};
    std::vector<Rml::byte> pixels(static_cast<size_t>(image->w) * static_cast<size_t>(image->h) * 4);
    for (int y = 0; y < image->h; ++y) {
      const auto* source_row = static_cast<const unsigned char*>(image->pixels) + y * image->pitch;
      auto* row = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(image->w) * 4;
      for (int x = 0; x < image->w; ++x) {
        const auto* source_pixel = source_row + x * 4;
        auto* pixel = row + x * 4;
        for (int c = 0; c < 3; ++c)
          pixel[c] = static_cast<Rml::byte>((static_cast<unsigned>(source_pixel[c]) * source_pixel[3] + 127) / 255);
        pixel[3] = source_pixel[3];
      }
    }
    return GenerateTexture({pixels.data(), pixels.size()}, dimensions);
  }

  void ReleaseTexture(Rml::TextureHandle texture) override { textures.erase(texture); }
  void EnableScissorRegion(bool enable) override { clipped = enable; }
  void SetScissorRegion(Rml::Rectanglei region) override { clip = region; }
  void SetTransform(const Rml::Matrix4f* value) override {
    transform = value ? *value : Rml::Matrix4f::Identity();
  }
};

RmlRenderer* create_rml_renderer(rhi::IDevice* device, rhi::Format format) {
  if (!device) return nullptr;
  auto renderer = std::make_unique<RmlRenderer>(); renderer->device = device;
  renderer->validation_diagnostics = SDL_getenv("OCTARYN_CLIENT_RHI_VALIDATION") != nullptr;
  Slang::ComPtr<rhi::IShaderProgram> program;
  const char* entries[] = {"vertex_main", "fragment_main"};
  if (!create_rhi_program(device, "octaryn-client/Shaders/Ui/Rml.slang", entries, 2, program)) return nullptr;
  rhi::ColorTargetDesc target{}; target.format = format; target.enableBlend = true;
  target.color.srcFactor = target.alpha.srcFactor = rhi::BlendFactor::One;
  target.color.dstFactor = target.alpha.dstFactor = rhi::BlendFactor::InvSrcAlpha;
  rhi::RenderPipelineDesc desc{}; desc.program = program; desc.targets = &target; desc.targetCount = 1;
  desc.depthStencil.depthTestEnable = false; desc.depthStencil.depthWriteEnable = false;
  desc.rasterizer.scissorEnable = true; desc.rasterizer.cullMode = rhi::CullMode::None;
  if (SLANG_FAILED(device->createRenderPipeline(desc, renderer->pipeline.writeRef()))) return nullptr;
  rhi::SamplerDesc sampler{};
  sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Point;
  sampler.addressU = sampler.addressV = sampler.addressW = rhi::TextureAddressingMode::ClampToEdge;
  if (SLANG_FAILED(device->createSampler(sampler, renderer->sampler.writeRef()))) return nullptr;
  const std::array<Rml::byte, 4> pixel{255, 255, 255, 255};
  const auto white = renderer->GenerateTexture({pixel.data(), pixel.size()}, {1, 1});
  if (!white) return nullptr;
  renderer->white = renderer->textures.at(white);
  renderer->textures.erase(white);
  return renderer.release();
}

Rml::RenderInterface* rml_render_interface(RmlRenderer* renderer) { return renderer; }

bool render_rml(RmlRenderer* renderer, rhi::ICommandEncoder* commands, rhi::ITextureView* target,
                Rml::Context* context, int width, int height) {
  if (!context) return true;
  if (!renderer || !commands || !target || width <= 0 || height <= 0) return false;
  auto& r = *renderer;
  r.draws.clear(); r.clipped = false; r.transform = Rml::Matrix4f::Identity();
  if (!context->Render() || r.failed) return false;
  if (r.draws.empty()) return true;
  const auto dimensions = context->GetDimensions();
  if (dimensions.x <= 0 || dimensions.y <= 0) return true;
  const float scale_x = static_cast<float>(width) / static_cast<float>(dimensions.x);
  const float scale_y = static_cast<float>(height) / static_cast<float>(dimensions.y);
  rhi::RenderPassColorAttachment color{}; color.view = target;
  color.loadOp = rhi::LoadOp::Load; color.storeOp = rhi::StoreOp::Store;
  rhi::RenderPassDesc pass{}; pass.colorAttachments = &color; pass.colorAttachmentCount = 1;
  auto* encoder = commands->beginRenderPass(pass);
  if (!encoder) return false;
  rhi::RenderState state{};
  state.viewports[0] = rhi::Viewport::fromSize(static_cast<float>(width), static_cast<float>(height));
  state.viewportCount = 1; state.scissorRectCount = 1; state.indexFormat = rhi::IndexFormat::Uint32;
  bool success = true;
  for (auto& draw : r.draws) {
    int x0 = 0, y0 = 0, x1 = width, y1 = height;
    if (draw.clipped) {
      x0 = static_cast<int>(std::clamp(std::floor(static_cast<float>(draw.clip.Left()) * scale_x), 0.f, static_cast<float>(width)));
      y0 = static_cast<int>(std::clamp(std::floor(static_cast<float>(draw.clip.Top()) * scale_y), 0.f, static_cast<float>(height)));
      x1 = static_cast<int>(std::clamp(std::ceil(static_cast<float>(draw.clip.Right()) * scale_x), 0.f, static_cast<float>(width)));
      y1 = static_cast<int>(std::clamp(std::ceil(static_cast<float>(draw.clip.Bottom()) * scale_y), 0.f, static_cast<float>(height)));
    }
    if (x1 <= x0 || y1 <= y0) continue;
    state.scissorRects[0] = {static_cast<uint32_t>(x0), static_cast<uint32_t>(y0),
                            static_cast<uint32_t>(x1), static_cast<uint32_t>(y1)};
    state.indexBuffer = {draw.geometry->indices, 0};
    encoder->setRenderState(state);
    draw.uniforms[18] = 2.f / static_cast<float>(dimensions.x);
    draw.uniforms[19] = 2.f / static_cast<float>(dimensions.y);
    auto* root = encoder->bindPipeline(r.pipeline);
    success = root && SLANG_SUCCEEDED(root->setBinding({0, 0, 0}, rhi::Binding(draw.geometry->vertices))) &&
        SLANG_SUCCEEDED(root->setBinding({0, 1, 0}, rhi::Binding(draw.texture->view))) &&
        SLANG_SUCCEEDED(root->setBinding({0, 2, 0}, rhi::Binding(r.sampler))) &&
        SLANG_SUCCEEDED(root->setData({0, 0, 0}, draw.uniforms.data(), sizeof(draw.uniforms)));
    if (!success) break;
    rhi::DrawArguments arguments{}; arguments.vertexCount = draw.geometry->count;
    encoder->drawIndexed(arguments);
  }
  encoder->end();
  if (success) ++r.frames;
  if (success && report_rml_frame(r.frames, r.validation_diagnostics)) {
    std::printf("rml_ui renderer=slang-rhi frame=%llu draws=%zu geometries=%zu geometry_peak=%zu "
                "compiled=%llu textures=%zu texture_peak=%zu premultiplied_alpha=1 geometry_memory=upload\n",
        static_cast<unsigned long long>(r.frames), r.draws.size(), r.geometries.size(), r.geometry_peak,
        static_cast<unsigned long long>(r.compiled_count), r.textures.size(), r.texture_peak);
  }
  return success;
}

void destroy_rml_renderer(RmlRenderer* renderer) { delete renderer; }
}
