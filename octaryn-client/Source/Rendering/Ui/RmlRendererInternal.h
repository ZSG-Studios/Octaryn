#pragma once

#include <slang-rhi.h>

#include <RmlUi/Core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "RmlProfile.h"
#include "RmlGeometryKey.h"
#include <list>

namespace octaryn::client::rendering {

// Shared internals of the slang-rhi RmlUi renderer. The renderer submits
// immediately during Rml::Context::Render because layers, clip masks, and
// filter composites interleave render-target switches with geometry draws.
// All passes attach the frame-sized stencil texture so the clip mask works on
// every layer, and all pipelines declare that depth-stencil format.
struct RmlRenderer;

struct RmlGeometry {
  Slang::ComPtr<rhi::IBuffer> vertices, indices;
  uint32_t count{};
  std::array<float,4> tint{1,1,1,1};
};
struct RmlCachedGeometry {
  RmlGeometryKey key;
  std::shared_ptr<RmlGeometry> geometry;
  std::uint64_t charge{};
};

struct RmlTexture {
  Slang::ComPtr<rhi::ITexture> image;
  Slang::ComPtr<rhi::ITextureView> view;
  uint32_t width{}, height{};
  bool wrap{};
  bool linear{};
  bool straight{};
};

// Offscreen frame-sized layer; index 0 is the base layer wrapping the
// caller's target view (no owned texture).
struct RmlLayer {
  Slang::ComPtr<rhi::ITexture> texture;
  Slang::ComPtr<rhi::ITextureView> target_view;
  Slang::ComPtr<rhi::ITextureView> sampled_view;
  Slang::ComPtr<rhi::ITexture> filter_stencil;
  Slang::ComPtr<rhi::ITextureView> filter_stencil_view;
  int origin_x{},origin_y{},width{},height{};
  std::uint64_t allocation_bytes{};
  rhi::ITextureView* base_view{};
  bool initialized{};
};

enum class RmlFilterType { Invalid = 0, Passthrough, Blur, DropShadow, ColorMatrix, MaskImage };
struct RmlFilter {
  RmlFilterType type{RmlFilterType::Invalid};
  float blend_factor{1.0f};
  float sigma{};
  float offset_x{}, offset_y{};
  float color[4]{};
  std::array<float, 16> color_matrix{};
};

enum class RmlShaderType { Invalid = 0, Gradient, Creation };
struct RmlShader {
  RmlShaderType type{RmlShaderType::Invalid};
  int gradient_function{};
  float p_x{}, p_y{}, v_x{}, v_y{};
  std::vector<float> stop_positions;
  std::vector<std::array<float, 4>> stop_colors;
  float creation_value{};
  float dimensions_x{}, dimensions_y{};
};

struct RmlRenderer final : Rml::RenderInterface {
  RmlProfile profile;
  std::list<RmlCachedGeometry> geometry_cache;
  std::uint64_t geometry_cache_bytes{},geometry_cache_hits{},geometry_cache_misses{};
  Slang::ComPtr<rhi::IDevice> device;
  rhi::Format format{rhi::Format::Undefined};

  // Pipelines: main draw, clip-masked draw, stencil mask writes, blits,
  // filters, and shader decorators.
  Slang::ComPtr<rhi::IRenderPipeline> pipeline, pipeline_clip;
  Slang::ComPtr<rhi::IRenderPipeline> mask_replace, mask_incr;
  Slang::ComPtr<rhi::IRenderPipeline> blit, blit_replace, blit_clip, blit_replace_clip;
  Slang::ComPtr<rhi::IRenderPipeline> color_matrix, blur, drop_shadow, blend_mask;
  Slang::ComPtr<rhi::IRenderPipeline> gradient, gradient_clip, creation, creation_clip;
  Slang::ComPtr<rhi::ISampler> sampler_point, sampler_linear, sampler_point_wrap, sampler_linear_wrap;

  std::unordered_map<Rml::CompiledGeometryHandle, std::shared_ptr<RmlGeometry>> geometries;
  std::unordered_map<Rml::TextureHandle, std::shared_ptr<RmlTexture>> textures;
  std::unordered_map<Rml::CompiledFilterHandle, std::shared_ptr<RmlFilter>> filters;
  std::unordered_map<Rml::CompiledShaderHandle, std::shared_ptr<RmlShader>> shaders;
  std::shared_ptr<RmlTexture> white;

  // Frame-scoped state, armed by render_rml.
  rhi::ICommandEncoder* commands{};
  rhi::IRenderPassEncoder* pass{};
  std::vector<RmlLayer> layers;
  uint32_t stack_depth{};
  uint32_t current_layer{};
  RmlLayer post_a, post_b, post_c, mask_layer;
  std::vector<RmlLayer> filter_surface_pool;
  Slang::ComPtr<rhi::ITexture> stencil_texture;
  Slang::ComPtr<rhi::ITextureView> stencil_view;
  int width{}, height{};
  int context_width{}, context_height{};
  int target_origin_x{},target_origin_y{},target_width{},target_height{};
  bool stencil_ready{};

  // Immediate-mode draw state mirrored from the RmlUi callbacks.
  Rml::Matrix4f transform{Rml::Matrix4f::Identity()};
  Rml::Rectanglei clip;
  bool clipped{};
  bool clip_mask_enabled{};
  uint32_t clip_ref{};

  Rml::CompiledGeometryHandle next_geometry{1};
  Rml::TextureHandle next_texture{1};
  Rml::CompiledFilterHandle next_filter{1};
  Rml::CompiledShaderHandle next_shader{1};
  uint64_t frames{}, compiled_count{};
  uint64_t layer_pushes{}, filter_passes{}, blur_passes{}, shadow_passes{}, mask_passes{}, clip_writes{};
  uint64_t filter_allocation_peak{},filter_surface_creates{},filter_surface_reuses{};
  size_t geometry_peak{}, texture_peak{};
  bool failed{}, validation_diagnostics{};

  // --- Basic interface (RmlRenderer.cpp) ---
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                              Rml::Span<const int> indices) override;
  void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                      Rml::TextureHandle texture) override;
  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> bytes, Rml::Vector2i dimensions) override;
  Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
  void ReleaseTexture(Rml::TextureHandle texture) override;
  void EnableScissorRegion(bool enable) override;
  void SetScissorRegion(Rml::Rectanglei region) override;
  void SetTransform(const Rml::Matrix4f* value) override;

  // --- Layers (RmlLayers.cpp) ---
  Rml::LayerHandle PushLayer() override;
  void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
                       Rml::BlendMode blend_mode,
                       Rml::Span<const Rml::CompiledFilterHandle> filters) override;
  void PopLayer() override;
  Rml::TextureHandle SaveLayerAsTexture() override;
  Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;

  // --- Clip mask (RmlClipMask.cpp) ---
  void EnableClipMask(bool enable) override;
  void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation) override;

  // --- Filters (RmlFilters.cpp) ---
  Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
  void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

  // --- Shader decorators (RmlShaders.cpp) ---
  Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) override;
  void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry,
                    Rml::Vector2f translation, Rml::TextureHandle texture) override;
  void ReleaseShader(Rml::CompiledShaderHandle shader) override;
};

// Shared helpers, defined in RmlRenderer.cpp.
namespace rml_internal {
void report_filter_memory(RmlRenderer&);

// Scissor rect in target pixels for the current clip state.
rhi::ScissorRect scissor_pixels(const RmlRenderer& renderer);

// Pass management: begin_layer_pass ends any open pass first. Passes attach
// the frame stencil texture; the first pass of a frame clears stencil to 0.
bool begin_layer_pass(RmlRenderer& renderer, uint32_t layer, bool clear_color);
void end_pass(RmlRenderer& renderer);
bool ensure_pass(RmlRenderer& renderer);

// Draws one compiled geometry with the given pipeline and uniform constants.
// The texture/sampler pair binds only when texture is non-null.
bool draw_geometry(RmlRenderer& renderer, rhi::IRenderPipeline* pipeline,
                   const RmlGeometry& geometry, const float* uniforms, size_t uniform_size,
                   rhi::ITextureView* texture = nullptr,rhi::ISampler* sampler = nullptr);

// Draws one fullscreen-rect blit into dst of the current layer pass.
bool blit(RmlRenderer& renderer, rhi::IRenderPipeline* pipeline,
          rhi::ITextureView* source, const Rml::Rectanglei& dst_pixels,
          const float* uniforms, size_t uniform_size,
          rhi::ITextureView* mask = nullptr);

// Frame-sized texture+views for layers/postprocess, recreated on demand.
bool ensure_surface(RmlRenderer& renderer, RmlLayer& surface, bool render_target,
                    const Rml::Rectanglei* bounds = nullptr);
std::array<float,4> surface_uv(const RmlRenderer&,const RmlLayer&,const Rml::Rectanglei&);
rhi::ITextureView* layer_target(RmlRenderer& renderer, uint32_t layer);
rhi::ITextureView* layer_source(RmlRenderer& renderer, uint32_t layer);

// Pass onto an arbitrary offscreen surface (postprocess, mask) without
// changing the layer stack position.
bool begin_surface_pass(RmlRenderer& renderer, RmlLayer& surface, bool clear_color);

// Filter passes (RmlFilters.cpp). apply_blur leaves the result in front;
// color matrix and drop shadow render into back and the caller swaps.
bool apply_color_matrix(RmlRenderer& renderer, RmlLayer& source, RmlLayer& target,
                        const Rml::Rectanglei& region, const RmlFilter& filter);
bool apply_blur(RmlRenderer& renderer, RmlLayer& front, RmlLayer& back,
                const Rml::Rectanglei& region, const RmlFilter& filter);
bool apply_drop_shadow(RmlRenderer& renderer, RmlLayer& source, RmlLayer& target,
                       const Rml::Rectanglei& region, const RmlFilter& filter);

} // namespace rml_internal

} // namespace octaryn::client::rendering
