#include "RmlRendererInternal.h"
#include <algorithm>

namespace octaryn::client::rendering {

// Stencil clip mask, mirroring the reference renderer's semantics on the
// frame stencil texture: the mask level counts nesting depth, geometry draws
// test Equal against it, and Intersect raises the level for nested masks.
void RmlRenderer::EnableClipMask(bool enable) {
  clip_mask_enabled = enable;
}

void RmlRenderer::RenderToClipMask(Rml::ClipMaskOperation operation,
                                   Rml::CompiledGeometryHandle geometry,
                                   Rml::Vector2f translation) {
  if (failed) return;
  const auto it = geometries.find(geometry);
  if (it == geometries.end()) {
    failed = true;
    return;
  }

  uint32_t write_value = 1;
  switch (operation) {
    case Rml::ClipMaskOperation::Set:
      // Reset the mask to empty, then write level 1 inside the geometry.
      rml_internal::end_pass(*this);
      commands->clearTextureDepthStencil(stencil_texture.get(), {0, 1, 0, 1}, false, 1.0f, true, 0);
      clip_ref = 1;
      break;
    case Rml::ClipMaskOperation::SetInverse:
      // Start from "inside everywhere", then cut the geometry out.
      rml_internal::end_pass(*this);
      commands->clearTextureDepthStencil(stencil_texture.get(), {0, 1, 0, 1}, false, 1.0f, true, 1);
      clip_ref = 1;
      write_value = 0;
      break;
    case Rml::ClipMaskOperation::Intersect:
      // Raise the mask level where the geometry overlaps the current mask.
      clip_ref += 1;
      write_value = clip_ref;
      break;
  }

  std::array<float, 28> uniforms{};
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      uniforms[static_cast<size_t>(row * 4 + column)] = transform.GetRow(row)[column];
  uniforms[16] = translation.x;
  uniforms[17] = translation.y;
  uniforms[18] = 2.f / static_cast<float>(context_width);
  uniforms[19] = 2.f / static_cast<float>(context_height);
  std::copy(it->second->tint.begin(),it->second->tint.end(),uniforms.begin()+24);

  if (!rml_internal::ensure_pass(*this)) {
    failed = true;
    return;
  }
  rhi::RenderState state{};
  state.viewports[0] = rhi::Viewport::fromSize(static_cast<float>(width),
                                               static_cast<float>(height));
  state.viewportCount = 1;
  state.scissorRects[0] = rml_internal::scissor_pixels(*this);
  state.scissorRectCount = 1;
  state.indexFormat = rhi::IndexFormat::Uint32;
  state.indexBuffer = {it->second->indices, 0};
  state.stencilRef = operation == Rml::ClipMaskOperation::Intersect
                         ? clip_ref - 1
                         : write_value;
  pass->setRenderState(state);
  auto* root = pass->bindPipeline(operation == Rml::ClipMaskOperation::Intersect
                                      ? mask_incr.get()
                                      : mask_replace.get());
  bool ok = root &&
      SLANG_SUCCEEDED(root->setBinding({0, 0, 0}, rhi::Binding(it->second->vertices))) &&
      SLANG_SUCCEEDED(root->setBinding({0, 1, 0}, rhi::Binding(white->view.get()))) &&
      SLANG_SUCCEEDED(root->setBinding({0, 2, 0}, rhi::Binding(sampler_point))) &&
      SLANG_SUCCEEDED(root->setData({0, 0, 0}, uniforms.data(), uniforms.size() * sizeof(float)));
  if (!ok) {
    failed = true;
    return;
  }
  rhi::DrawArguments arguments{};
  arguments.vertexCount = it->second->count;
  pass->drawIndexed(arguments);
  ++clip_writes;
}

} // namespace octaryn::client::rendering
