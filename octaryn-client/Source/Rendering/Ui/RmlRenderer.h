#pragma once
#include <slang-rhi.h>
#include <cstdint>

namespace Rml { class Context; class RenderInterface; }
namespace octaryn::client::rendering {
struct RmlRenderer;
RmlRenderer* create_rml_renderer(rhi::IDevice*, rhi::Format);
Rml::RenderInterface* rml_render_interface(RmlRenderer*);
bool render_rml(RmlRenderer*, rhi::ICommandEncoder*, rhi::ITextureView*,
                Rml::Context*, int width, int height);
void destroy_rml_renderer(RmlRenderer*);

// Advanced RmlUi surface this backend implements; every field is 1 when the
// renderer initialized its full pipeline set, on both the DX12 and Vulkan
// slang-rhi backends.
struct RmlRendererFeatures {
  std::uint32_t layers{};             // PushLayer/PopLayer/CompositeLayers, SaveLayerAsTexture
  std::uint32_t clip_mask{};          // stencil clip masks, incl. nested Intersect
  std::uint32_t filters{};            // blur, drop-shadow, opacity, color-matrix family, mask images
  std::uint32_t shader_decorators{};  // linear/radial/conic gradients, creation shader
};
RmlRendererFeatures rml_renderer_features(RmlRenderer*);
}
