#include "RmlRenderer.h"
#include "RmlRendererInternal.h"
#include "RmlTextureSource.h"
#include "RhiShader.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

namespace octaryn::client::rendering {
namespace {

constexpr bool report_rml_frame(uint64_t frame, bool validation) {
  return frame == 1 || (validation && (frame == 301 || frame == 601));
}

// All UI passes attach this stencil-capable depth-stencil texture, so every
// pipeline declares it even when stencil testing is off.
constexpr rhi::Format kStencilFormat = rhi::Format::D32FloatS8Uint;

struct PipelineOptions {
  bool clip_test{};
  bool mask_write_replace{};
  bool mask_write_incr{};
  bool no_color_writes{};
  bool no_blend{};
};

rhi::RenderPipelineDesc make_pipeline_desc(rhi::IShaderProgram* program, rhi::Format format,
                                           rhi::ColorTargetDesc& target,
                                           PipelineOptions options) {
  rhi::RenderPipelineDesc desc{};
  desc.program = program;
  target = rhi::ColorTargetDesc{};
  target.format = format;
  target.enableBlend = !options.no_blend && !options.no_color_writes;
  target.color.srcFactor = target.alpha.srcFactor = rhi::BlendFactor::One;
  target.color.dstFactor = target.alpha.dstFactor = rhi::BlendFactor::InvSrcAlpha;
  if (options.no_color_writes) target.writeMask = rhi::RenderTargetWriteMask::None;
  desc.targets = &target;
  desc.targetCount = 1;
  desc.depthStencil.format = kStencilFormat;
  desc.depthStencil.depthTestEnable = false;
  desc.depthStencil.depthWriteEnable = false;
  desc.rasterizer.scissorEnable = true;
  desc.rasterizer.cullMode = rhi::CullMode::None;
  desc.primitiveTopology = rhi::PrimitiveTopology::TriangleList;
  if (options.clip_test) {
    desc.depthStencil.stencilEnable = true;
    desc.depthStencil.stencilReadMask = 0xFF;
    desc.depthStencil.stencilWriteMask = 0;
    desc.depthStencil.frontFace.stencilFunc = rhi::ComparisonFunc::Equal;
    desc.depthStencil.frontFace.stencilFailOp = rhi::StencilOp::Keep;
    desc.depthStencil.frontFace.stencilDepthFailOp = rhi::StencilOp::Keep;
    desc.depthStencil.frontFace.stencilPassOp = rhi::StencilOp::Keep;
    desc.depthStencil.backFace = desc.depthStencil.frontFace;
  } else if (options.mask_write_replace || options.mask_write_incr) {
    desc.depthStencil.stencilEnable = true;
    desc.depthStencil.stencilReadMask = 0xFF;
    desc.depthStencil.stencilWriteMask = 0xFF;
    desc.depthStencil.frontFace.stencilFunc = options.mask_write_incr
                                                  ? rhi::ComparisonFunc::Equal
                                                  : rhi::ComparisonFunc::Always;
    desc.depthStencil.frontFace.stencilFailOp = rhi::StencilOp::Keep;
    desc.depthStencil.frontFace.stencilDepthFailOp = rhi::StencilOp::Keep;
    desc.depthStencil.frontFace.stencilPassOp = options.mask_write_incr
                                                    ? rhi::StencilOp::IncrementSaturate
                                                    : rhi::StencilOp::Replace;
    desc.depthStencil.backFace = desc.depthStencil.frontFace;
  }
  return desc;
}

bool create_pipeline(rhi::IDevice* device, rhi::IShaderProgram* program, rhi::Format format,
                     PipelineOptions options, Slang::ComPtr<rhi::IRenderPipeline>& out) {
  rhi::ColorTargetDesc target{};
  auto desc = make_pipeline_desc(program, format, target, options);
  return SLANG_SUCCEEDED(device->createRenderPipeline(desc, out.writeRef()));
}

} // namespace



// ---------------------------------------------------------------------------
// Rml::RenderInterface: basic surface
// ---------------------------------------------------------------------------

Rml::CompiledGeometryHandle RmlRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                         Rml::Span<const int> indices) {
  RmlScopeTimer timer(profile,RmlWork::Geometry);
  static_assert(sizeof(Rml::Vertex) == 20 && offsetof(Rml::Vertex, colour) == 8 &&
                offsetof(Rml::Vertex, tex_coord) == 12 && sizeof(int) == 4);
  if (vertices.empty() || indices.empty() ||
      indices.size() > std::numeric_limits<uint32_t>::max()) return 0;
  for (const int index : indices)
    if (index < 0 || static_cast<size_t>(index) >= vertices.size()) { failed = true; return 0; }
  const auto vertex_bytes=std::as_bytes(std::span(vertices.data(),vertices.size()));
  const auto index_bytes=std::as_bytes(std::span(indices.data(),indices.size()));
  RmlGeometryKey key;
  const bool cacheable=canonical_rml_geometry(vertex_bytes,index_bytes,key);
  auto geometry = std::make_shared<RmlGeometry>();
  bool reused=false;
  if(cacheable) {
    auto found=std::find_if(geometry_cache.begin(),geometry_cache.end(),[&](const auto& entry) {
      return entry.key.hash==key.hash && entry.key.vertices==key.vertices && entry.key.indices==key.indices;
    });
    if(found!=geometry_cache.end()) {
      *geometry=*found->geometry;
      geometry_cache.splice(geometry_cache.begin(),geometry_cache,found);
      reused=true;++geometry_cache_hits;
    }
    geometry->tint=key.tint;
  }
  if(!reused) {
    ++geometry_cache_misses;
    if(profile.enabled)profile.geometry_bytes+=vertex_bytes.size()+index_bytes.size();
    rhi::BufferDesc desc{};
    // Buffers are immutable. Retained handles and submitted commands keep their
    // own references when the bounded cache evicts an entry.
    desc.memoryType = rhi::MemoryType::Upload;
    desc.size = vertex_bytes.size();
    desc.usage = rhi::BufferUsage::ShaderResource | rhi::BufferUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::ShaderResource;
    if (SLANG_FAILED(device->createBuffer(desc, cacheable?static_cast<const void*>(key.vertices.data()):static_cast<const void*>(vertices.data()), geometry->vertices.writeRef()))) {
      failed = true; return 0;
    }
    desc.size = index_bytes.size();
    desc.elementSize = sizeof(int);
    desc.usage = rhi::BufferUsage::IndexBuffer | rhi::BufferUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::IndexBuffer;
    if (SLANG_FAILED(device->createBuffer(desc, indices.data(), geometry->indices.writeRef()))) {
      failed = true; return 0;
    }
    geometry->count = static_cast<uint32_t>(indices.size());
    if(cacheable) {
      // Account two 64 KiB-rounded buffer requests plus exact CPU key bytes.
      // This bounds cache retention; live/in-flight references have separate lifetimes.
      const auto rounded=[](std::size_t size){return (std::uint64_t(size)+65535)&~std::uint64_t(65535);};
      const auto charge=rounded(vertex_bytes.size())+rounded(index_bytes.size())+vertex_bytes.size()+index_bytes.size();
      while(!geometry_cache.empty() && (geometry_cache.size()>=512 || geometry_cache_bytes+charge>64ull*1024*1024)) {
        geometry_cache_bytes-=geometry_cache.back().charge;geometry_cache.pop_back();
      }
      geometry_cache_bytes+=charge;
      geometry_cache.push_front({std::move(key),geometry,charge});
    }
  }
  const auto handle = next_geometry++;
  geometries.emplace(handle, std::move(geometry));
  ++compiled_count;
  geometry_peak = std::max(geometry_peak, geometries.size());
  return handle;
}

void RmlRenderer::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
                                 Rml::TextureHandle texture) {
  const auto geometry = geometries.find(handle);
  const auto image = textures.find(texture);
  if (geometry == geometries.end() || (texture && image == textures.end())) {
    failed = true;
    return;
  }
  std::array<float, 28> uniforms{};
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      uniforms[static_cast<size_t>(row * 4 + column)] = transform.GetRow(row)[column];
  uniforms[16] = translation.x;
  uniforms[17] = translation.y;
  uniforms[18] = 2.f / static_cast<float>(context_width);
  uniforms[19] = 2.f / static_cast<float>(context_height);
  uniforms[20] = texture && image->second->straight ? 1.f : 0.f;
  std::copy(geometry->second->tint.begin(),geometry->second->tint.end(),uniforms.begin()+24);
  auto* pipeline = clip_mask_enabled ? pipeline_clip.get() : this->pipeline.get();
  auto* sampler = sampler_point.get();
  if (texture) {
    const auto& policy = *image->second;
    sampler = policy.linear ? (policy.wrap ? sampler_linear_wrap.get() : sampler_linear.get())
                            : (policy.wrap ? sampler_point_wrap.get() : sampler_point.get());
  }
  if (!rml_internal::draw_geometry(*this, pipeline, *geometry->second, uniforms.data(),
                                   uniforms.size() * sizeof(float),
                                   texture ? image->second->view.get()
                                           : white->view.get(),
                                   sampler)) {
    failed = true;
  }
}

void RmlRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) { geometries.erase(geometry); }

Rml::TextureHandle RmlRenderer::GenerateTexture(Rml::Span<const Rml::byte> bytes,
                                                Rml::Vector2i dimensions) {
  RmlScopeTimer timer(profile,RmlWork::TextureUpload);
  if(profile.enabled)profile.texture_bytes+=bytes.size();
  if (dimensions.x <= 0 || dimensions.y <= 0 ||
      static_cast<uint64_t>(dimensions.x) * static_cast<uint64_t>(dimensions.y) * 4 != bytes.size()) {
    failed = true; return 0;
  }
  auto texture = std::make_shared<RmlTexture>();
  rhi::TextureDesc desc{};
  desc.size = {static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y), 1};
  desc.format = rhi::Format::RGBA8Unorm;
  desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::CopyDestination;
  desc.defaultState = rhi::ResourceState::ShaderResource;
  rhi::SubresourceData pixels{};
  pixels.data = bytes.data();
  pixels.rowPitch = static_cast<rhi::Size>(dimensions.x) * 4;
  pixels.slicePitch = pixels.rowPitch * static_cast<rhi::Size>(dimensions.y);
  if (SLANG_FAILED(device->createTexture(desc, &pixels, texture->image.writeRef())) ||
      SLANG_FAILED(texture->image->getDefaultView(texture->view.writeRef()))) {
    failed = true; return 0;
  }
  texture->width = static_cast<uint32_t>(dimensions.x);
  texture->height = static_cast<uint32_t>(dimensions.y);
  const auto handle = next_texture++;
  textures.emplace(handle, std::move(texture));
  texture_peak = std::max(texture_peak, textures.size());
  return handle;
}

Rml::TextureHandle RmlRenderer::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) {
  RmlScopeTimer timer(profile,RmlWork::TextureLoad);
  const auto policy = rml_texture_source(source);
  const auto filename = Rml::String(policy.filename);
  std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> loaded(SDL_LoadPNG(filename.c_str()), SDL_DestroySurface);
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
        pixel[c] = policy.straight ? source_pixel[c] :
            static_cast<Rml::byte>((static_cast<unsigned>(source_pixel[c]) * source_pixel[3] + 127) / 255);
      pixel[3] = source_pixel[3];
    }
  }
  const auto handle=GenerateTexture({pixels.data(), pixels.size()}, dimensions);
  if(handle) { textures.at(handle)->wrap=policy.wrap; textures.at(handle)->linear=policy.linear; textures.at(handle)->straight=policy.straight; }
  return handle;
}

void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture) { textures.erase(texture); }
void RmlRenderer::EnableScissorRegion(bool enable) { clipped = enable; }
void RmlRenderer::SetScissorRegion(Rml::Rectanglei region) { clip = region; }
void RmlRenderer::SetTransform(const Rml::Matrix4f* value) {
  transform = value ? *value : Rml::Matrix4f::Identity();
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

RmlRenderer* create_rml_renderer(rhi::IDevice* device, rhi::Format format) {
  if (!device) return nullptr;
  auto renderer = std::make_unique<RmlRenderer>();
  renderer->device = device;
  renderer->format = format;
  renderer->validation_diagnostics = SDL_getenv("OCTARYN_CLIENT_RHI_VALIDATION") != nullptr;
  renderer->profile.enabled=SDL_getenv("OCTARYN_CLIENT_UI_PROFILE")!=nullptr;

  auto* d = device;
  auto load = [&](const char* path, Slang::ComPtr<rhi::IShaderProgram>& program) {
    const char* entries[] = {"vertex_main", "fragment_main"};
    return create_rhi_program(d, path, entries, 2, program);
  };
  Slang::ComPtr<rhi::IShaderProgram> main_program, blit_program, matrix_program,
      blur_program, shadow_program, mask_program, gradient_program, creation_program;
  if (!load("octaryn-client/Shaders/Ui/Rml.slang", main_program) ||
      !load("octaryn-client/Shaders/Ui/RmlBlit.slang", blit_program) ||
      !load("octaryn-client/Shaders/Ui/RmlColorMatrix.slang", matrix_program) ||
      !load("octaryn-client/Shaders/Ui/RmlBlur.slang", blur_program) ||
      !load("octaryn-client/Shaders/Ui/RmlDropShadow.slang", shadow_program) ||
      !load("octaryn-client/Shaders/Ui/RmlBlendMask.slang", mask_program) ||
      !load("octaryn-client/Shaders/Ui/RmlGradient.slang", gradient_program) ||
      !load("octaryn-client/Shaders/Ui/RmlCreation.slang", creation_program)) {
    return nullptr;
  }

  const PipelineOptions plain{};
  const PipelineOptions clip{.clip_test = true};
  const PipelineOptions no_blend{.no_blend = true};
  const PipelineOptions no_blend_clip{.clip_test = true, .no_blend = true};
  const PipelineOptions mask_replace{.mask_write_replace = true, .no_color_writes = true};
  const PipelineOptions mask_incr{.mask_write_incr = true, .no_color_writes = true};

  auto* r = renderer.get();
  if (!create_pipeline(d, main_program, format, plain, r->pipeline) ||
      !create_pipeline(d, main_program, format, clip, r->pipeline_clip) ||
      !create_pipeline(d, main_program, format, mask_replace, r->mask_replace) ||
      !create_pipeline(d, main_program, format, mask_incr, r->mask_incr) ||
      !create_pipeline(d, blit_program, format, plain, r->blit) ||
      !create_pipeline(d, blit_program, format, no_blend, r->blit_replace) ||
      !create_pipeline(d, blit_program, format, clip, r->blit_clip) ||
      !create_pipeline(d, blit_program, format, no_blend_clip, r->blit_replace_clip) ||
      !create_pipeline(d, matrix_program, format, no_blend, r->color_matrix) ||
      !create_pipeline(d, blur_program, format, no_blend, r->blur) ||
      !create_pipeline(d, shadow_program, format, no_blend, r->drop_shadow) ||
      !create_pipeline(d, mask_program, format, no_blend, r->blend_mask) ||
      !create_pipeline(d, gradient_program, format, plain, r->gradient) ||
      !create_pipeline(d, gradient_program, format, clip, r->gradient_clip) ||
      !create_pipeline(d, creation_program, format, plain, r->creation) ||
      !create_pipeline(d, creation_program, format, clip, r->creation_clip)) {
    return nullptr;
  }

  rhi::SamplerDesc sampler{};
  sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Point;
  sampler.addressU = sampler.addressV = sampler.addressW = rhi::TextureAddressingMode::ClampToEdge;
  if (SLANG_FAILED(d->createSampler(sampler, r->sampler_point.writeRef()))) return nullptr;
  sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Linear;
  if (SLANG_FAILED(d->createSampler(sampler, r->sampler_linear.writeRef()))) return nullptr;
  sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Point;
  sampler.addressU = sampler.addressV = sampler.addressW = rhi::TextureAddressingMode::Wrap;
  if (SLANG_FAILED(d->createSampler(sampler, r->sampler_point_wrap.writeRef()))) return nullptr;
  sampler.minFilter = sampler.magFilter = sampler.mipFilter = rhi::TextureFilteringMode::Linear;
  if (SLANG_FAILED(d->createSampler(sampler, r->sampler_linear_wrap.writeRef()))) return nullptr;

  const std::array<Rml::byte, 4> pixel{255, 255, 255, 255};
  const auto white = r->GenerateTexture({pixel.data(), pixel.size()}, {1, 1});
  if (!white) return nullptr;
  r->white = r->textures.at(white);
  r->textures.erase(white);
  return renderer.release();
}

Rml::RenderInterface* rml_render_interface(RmlRenderer* renderer) { return renderer; }

bool render_rml(RmlRenderer* renderer, rhi::ICommandEncoder* commands, rhi::ITextureView* target,
                Rml::Context* context, int width, int height) {
  if (!context) return true;
  if (!renderer || !commands || !target || width <= 0 || height <= 0) return false;
  auto& r = *renderer;
  r.profile.reset();
  const auto profile_begin=r.profile.enabled?SDL_GetTicksNS():0;

  // Frame setup: arm immediate submission, reset layers and clip state.
  r.commands = commands;
  r.pass = nullptr;
  r.width = width;
  r.height = height;
  r.stack_depth = 0;
  r.current_layer = 0;
  r.clip_mask_enabled = false;
  r.clip_ref = 0;
  r.clipped = false;
  r.transform = Rml::Matrix4f::Identity();
  const auto dimensions = context->GetDimensions();
  if (dimensions.x <= 0 || dimensions.y <= 0) { r.commands = nullptr; return true; }
  r.context_width = dimensions.x;
  r.context_height = dimensions.y;

  // Frame stencil texture for clip masks; cleared by the first pass.
  if (r.stencil_texture) {
    const auto size = r.stencil_texture->getDesc().size;
    if (size.width != static_cast<uint32_t>(width) ||
        size.height != static_cast<uint32_t>(height)) {
      r.stencil_texture = nullptr;
      r.stencil_view = nullptr;
    }
  }
  if (!r.stencil_texture) {
    rhi::TextureDesc desc{};
    desc.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    desc.format = rhi::Format::D32FloatS8Uint;
    desc.usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::CopyDestination;
    desc.defaultState = rhi::ResourceState::DepthWrite;
    if (SLANG_FAILED(r.device->createTexture(desc, nullptr, r.stencil_texture.writeRef())) ||
        SLANG_FAILED(r.stencil_texture->getDefaultView(r.stencil_view.writeRef()))) {
      r.commands = nullptr;
      return false;
    }
  }
  r.stencil_ready = false;

  if (r.layers.empty()) r.layers.resize(1);
  r.layers[0].base_view = target;
  if (!rml_internal::begin_layer_pass(r, 0, false)) { r.commands = nullptr; return false; }

  const bool rendered = context->Render();
  rml_internal::end_pass(r);
  const bool ok = rendered && !r.failed;
  r.commands = nullptr;
  r.current_layer = 0;
  r.stack_depth = 0;

  if (ok) ++r.frames;
  if(r.profile.enabled) {
    r.profile.report(r.frames,SDL_GetTicksNS()-profile_begin);
    if(r.frames==1 || r.frames%60==0)std::printf("rml_geometry_cache frame=%llu hits=%llu misses=%llu entries=%zu accounted_bytes=%llu immutable=1\n",
      static_cast<unsigned long long>(r.frames),static_cast<unsigned long long>(r.geometry_cache_hits),
      static_cast<unsigned long long>(r.geometry_cache_misses),r.geometry_cache.size(),static_cast<unsigned long long>(r.geometry_cache_bytes));
  }
  if (ok && report_rml_frame(r.frames, r.validation_diagnostics)) {
    rml_internal::report_filter_memory(r);
    std::printf("rml_ui renderer=slang-rhi frame=%llu geometries=%zu geometry_peak=%zu "
                "compiled=%llu textures=%zu texture_peak=%zu layers=%zu advanced=1 "
                "layer_pushes=%llu filter_passes=%llu blur_passes=%llu shadow_passes=%llu "
                "mask_passes=%llu clip_writes=%llu premultiplied_alpha=1 geometry_memory=upload\n",
        static_cast<unsigned long long>(r.frames), r.geometries.size(), r.geometry_peak,
        static_cast<unsigned long long>(r.compiled_count), r.textures.size(), r.texture_peak,
        r.layers.size(), static_cast<unsigned long long>(r.layer_pushes),
        static_cast<unsigned long long>(r.filter_passes), static_cast<unsigned long long>(r.blur_passes),
        static_cast<unsigned long long>(r.shadow_passes), static_cast<unsigned long long>(r.mask_passes),
        static_cast<unsigned long long>(r.clip_writes));
  }
  return ok;
}

void destroy_rml_renderer(RmlRenderer* renderer) { delete renderer; }

RmlRendererFeatures rml_renderer_features(RmlRenderer* renderer) {
  RmlRendererFeatures features{};
  if (!renderer) return features;
  const bool ok = renderer->pipeline && renderer->pipeline_clip && renderer->blit &&
      renderer->color_matrix && renderer->blur && renderer->drop_shadow &&
      renderer->blend_mask && renderer->gradient && renderer->creation;
  features.layers = ok ? 1u : 0u;
  features.clip_mask = ok ? 1u : 0u;
  features.filters = ok ? 1u : 0u;
  features.shader_decorators = ok ? 1u : 0u;
  return features;
}

} // namespace octaryn::client::rendering
