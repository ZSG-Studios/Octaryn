#include "RmlRendererInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace octaryn::client::rendering {

namespace rml_internal {

rhi::ScissorRect scissor_pixels(const RmlRenderer& r) {
  if (!r.clipped) return rhi::ScissorRect::fromSize(
      static_cast<uint32_t>(r.width), static_cast<uint32_t>(r.height));
  const float scale_x = static_cast<float>(r.width) / static_cast<float>(r.context_width);
  const float scale_y = static_cast<float>(r.height) / static_cast<float>(r.context_height);
  const auto clamp_x = [&](float v) {
    return static_cast<uint32_t>(std::clamp(v, 0.f, static_cast<float>(r.width)));
  };
  const auto clamp_y = [&](float v) {
    return static_cast<uint32_t>(std::clamp(v, 0.f, static_cast<float>(r.height)));
  };
  return {clamp_x(std::floor(r.clip.Left() * scale_x)),
          clamp_y(std::floor(r.clip.Top() * scale_y)),
          clamp_x(std::ceil(r.clip.Right() * scale_x)),
          clamp_y(std::ceil(r.clip.Bottom() * scale_y))};
}

bool open_pass(RmlRenderer& r, rhi::ITextureView* view, bool clear_color,
               rhi::ITextureView* filter_stencil = nullptr) {
  end_pass(r);
  if (!view || !r.commands || !r.stencil_view) return false;
  rhi::RenderPassColorAttachment color{};
  color.view = view;
  color.loadOp = clear_color ? rhi::LoadOp::Clear : rhi::LoadOp::Load;
  color.storeOp = rhi::StoreOp::Store;
  rhi::RenderPassDepthStencilAttachment depth{};
  depth.view = filter_stencil?filter_stencil:r.stencil_view.get();
  depth.depthLoadOp = rhi::LoadOp::DontCare;
  depth.depthStoreOp = rhi::StoreOp::DontCare;
  depth.stencilLoadOp = !filter_stencil && r.stencil_ready ? rhi::LoadOp::Load : rhi::LoadOp::Clear;
  depth.stencilStoreOp = rhi::StoreOp::Store;
  depth.stencilClearValue = 0;
  rhi::RenderPassDesc pass{};
  pass.colorAttachments = &color;
  pass.colorAttachmentCount = 1;
  pass.depthStencilAttachment = &depth;
  r.pass = r.commands->beginRenderPass(pass);
  if (!r.pass) return false;
  if(!filter_stencil)r.stencil_ready = true;
  return true;
}

bool begin_layer_pass(RmlRenderer& r, uint32_t layer, bool clear_color) {
  rhi::ITextureView* target = layer_target(r, layer);
  if (!open_pass(r, target, clear_color)) return false;
  r.current_layer = layer;
  r.target_origin_x=r.target_origin_y=0;r.target_width=r.width;r.target_height=r.height;
  return true;
}

void end_pass(RmlRenderer& r) {
  if (r.pass) {
    r.pass->end();
    r.pass = nullptr;
  }
}

bool ensure_pass(RmlRenderer& r) {
  if (r.pass) return true;
  return begin_layer_pass(r, r.current_layer, false);
}

bool draw_geometry(RmlRenderer& r, rhi::IRenderPipeline* pipeline,
                   const RmlGeometry& geometry, const float* uniforms, size_t uniform_size,
                   rhi::ITextureView* texture) {
  if (!ensure_pass(r) || !pipeline) return false;
  rhi::RenderState state{};
  state.viewports[0] = rhi::Viewport::fromSize(static_cast<float>(r.width),
                                               static_cast<float>(r.height));
  state.viewports[0].originX=-float(r.target_origin_x);
  state.viewports[0].originY=-float(r.target_origin_y);
  state.viewportCount = 1;
  state.scissorRects[0] = scissor_pixels(r);
  state.scissorRectCount = 1;
  state.indexFormat = rhi::IndexFormat::Uint32;
  state.indexBuffer = {geometry.indices, 0};
  state.stencilRef = r.clip_ref;
  r.pass->setRenderState(state);
  auto* root = r.pass->bindPipeline(pipeline);
  if (!root) return false;
  bool ok = SLANG_SUCCEEDED(root->setBinding({0, 0, 0}, rhi::Binding(geometry.vertices)));
  if (ok && texture)
    ok = SLANG_SUCCEEDED(root->setBinding({0, 1, 0}, rhi::Binding(texture)));
  if (ok && texture)
    ok = SLANG_SUCCEEDED(root->setBinding({0, 2, 0}, rhi::Binding(r.sampler_point)));
  ok = ok && SLANG_SUCCEEDED(root->setData({0, 0, 0}, uniforms, uniform_size));
  if (!ok) return false;
  rhi::DrawArguments arguments{};
  arguments.vertexCount = geometry.count;
  r.pass->drawIndexed(arguments);
  return true;
}

bool blit(RmlRenderer& r, rhi::IRenderPipeline* pipeline, rhi::ITextureView* source,
          const Rml::Rectanglei& dst_pixels, const float* uniforms, size_t uniform_size,
          rhi::ITextureView* mask) {
  if (!ensure_pass(r) || !pipeline || !source) return false;
  rhi::RenderState state{};
  state.viewports[0] = rhi::Viewport::fromSize(static_cast<float>(r.width),
                                               static_cast<float>(r.height));
  state.viewports[0].originX=-float(r.target_origin_x);
  state.viewports[0].originY=-float(r.target_origin_y);
  state.viewportCount = 1;
  state.scissorRects[0] = {
      static_cast<uint32_t>(std::clamp(dst_pixels.Left()-r.target_origin_x,0,r.target_width)),
      static_cast<uint32_t>(std::clamp(dst_pixels.Top()-r.target_origin_y,0,r.target_height)),
      static_cast<uint32_t>(std::clamp(dst_pixels.Right()-r.target_origin_x,0,r.target_width)),
      static_cast<uint32_t>(std::clamp(dst_pixels.Bottom()-r.target_origin_y,0,r.target_height))};
  state.scissorRectCount = 1;
  if (state.scissorRects[0].maxX <= state.scissorRects[0].minX ||
      state.scissorRects[0].maxY <= state.scissorRects[0].minY) return true;
  state.stencilRef = r.clip_ref;
  r.pass->setRenderState(state);
  auto* root = r.pass->bindPipeline(pipeline);
  if (!root) return false;
  bool ok = SLANG_SUCCEEDED(root->setBinding({0, 0, 0}, rhi::Binding(source)));
  if (ok && mask)
    ok = SLANG_SUCCEEDED(root->setBinding({0, 1, 0}, rhi::Binding(mask)));
  ok = ok && SLANG_SUCCEEDED(root->setBinding(
                 {0, mask ? 2u : 1u, 0}, rhi::Binding(r.sampler_linear)));
  ok = ok && SLANG_SUCCEEDED(root->setData({0, 0, 0}, uniforms, uniform_size));
  if (!ok) return false;
  rhi::DrawArguments arguments{};
  arguments.vertexCount = 3;
  r.pass->draw(arguments);
  return true;
}

std::array<float,4> surface_uv(const RmlRenderer& r,const RmlLayer& surface,const Rml::Rectanglei& bounds) {
  const float width=float(surface.width?surface.width:r.width),height=float(surface.height?surface.height:r.height);
  return {float(bounds.Left()-surface.origin_x)/width,float(bounds.Top()-surface.origin_y)/height,
          float(bounds.Right()-surface.origin_x)/width,float(bounds.Bottom()-surface.origin_y)/height};
}
bool ensure_surface(RmlRenderer& r, RmlLayer& surface, bool render_target,const Rml::Rectanglei* bounds) {
  if (r.width <= 0 || r.height <= 0) return false;
  const char* reference=std::getenv("OCTARYN_CLIENT_UI_FULL_SIZE_SURFACES");
  const bool crop=bounds && !(reference && *reference=='1');
  const int origin_x=crop?bounds->Left():0,origin_y=crop?bounds->Top():0;
  const int width=crop?std::max(64,((bounds->Width()+63)/64)*64):r.width;
  const int height=crop?std::max(64,((bounds->Height()+63)/64)*64):r.height;
  if(surface.origin_x!=origin_x || surface.origin_y!=origin_y)surface.initialized=false;
  surface.origin_x=origin_x;surface.origin_y=origin_y;
  if (surface.texture) {
    const auto size = surface.texture->getDesc().size;
    if (size.width == static_cast<uint32_t>(width) &&
        size.height == static_cast<uint32_t>(height)) return true;
    if(bounds)r.filter_surface_pool.push_back(std::move(surface));
    surface = {};
  }
  if(bounds) {
    auto match=std::find_if(r.filter_surface_pool.begin(),r.filter_surface_pool.end(),[&](const auto& candidate) {
      return candidate.width==width && candidate.height==height && bool(candidate.filter_stencil)==crop;
    });
    if(match!=r.filter_surface_pool.end()) {
      surface=std::move(*match);r.filter_surface_pool.erase(match);
      ++r.filter_surface_reuses;
      surface.origin_x=origin_x;surface.origin_y=origin_y;surface.initialized=false;return true;
    }
    // Bound idle pool storage to two frame areas and eight reusable surfaces.
    auto pixels=[&]() {std::uint64_t sum=0;for(const auto& s:r.filter_surface_pool)sum+=std::uint64_t(s.width)*std::uint64_t(s.height);return sum;};
    while(r.filter_surface_pool.size()>8 || pixels()>2ull*std::uint64_t(r.width)*std::uint64_t(r.height))
      r.filter_surface_pool.erase(r.filter_surface_pool.begin());
  }
  rhi::TextureDesc desc{};
  desc.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
  desc.format = r.format;
  desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget |
               rhi::TextureUsage::CopySource | rhi::TextureUsage::CopyDestination;
  desc.defaultState = rhi::ResourceState::ShaderResource;
  if (SLANG_FAILED(r.device->createTexture(desc, nullptr, surface.texture.writeRef())))
    return false;
  if(bounds) {
    ++r.filter_surface_creates;
    rhi::Size size{},alignment{};
    if(SLANG_SUCCEEDED(r.device->getTextureAllocationInfo(desc,&size,&alignment)))surface.allocation_bytes=size;
  }
  // The default view serves both render-target and sampled use; the texture
  // usage flags above admit both roles.
  if (render_target &&
      SLANG_FAILED(surface.texture->getDefaultView(surface.target_view.writeRef())))
    return false;
  if (SLANG_FAILED(surface.texture->getDefaultView(surface.sampled_view.writeRef())))
    return false;
  surface.origin_x=origin_x;surface.origin_y=origin_y;surface.width=width;surface.height=height;
  if(crop) {
    desc.format=rhi::Format::D32FloatS8Uint;desc.usage=rhi::TextureUsage::DepthStencil;
    desc.defaultState=rhi::ResourceState::DepthWrite;
    if(SLANG_FAILED(r.device->createTexture(desc,nullptr,surface.filter_stencil.writeRef())) ||
       SLANG_FAILED(surface.filter_stencil->getDefaultView(surface.filter_stencil_view.writeRef())))return false;
    rhi::Size size{},alignment{};
    if(surface.allocation_bytes && SLANG_SUCCEEDED(r.device->getTextureAllocationInfo(desc,&size,&alignment)))surface.allocation_bytes+=size;
    else surface.allocation_bytes=0;
  }
  if(bounds && r.validation_diagnostics)std::printf("rml_filter_surface cropped=%u width=%d height=%d origin_x=%d origin_y=%d\n",crop?1u:0u,width,height,origin_x,origin_y);
  (void)render_target;
  return true;
}

void report_filter_memory(RmlRenderer& r) {
  std::uint64_t active=0,pooled=0;bool available=true;
  for(const auto* surface:{&r.post_a,&r.post_b,&r.post_c}) {
    active+=surface->allocation_bytes;
    available=available && (!surface->texture || surface->allocation_bytes>0);
  }
  for(const auto& surface:r.filter_surface_pool) {
    pooled+=surface.allocation_bytes;available=available && surface.allocation_bytes>0;
  }
  r.filter_allocation_peak=std::max(r.filter_allocation_peak,active+pooled);
  // Backend allocation requirements of owned textures; heap overhead and command
  // buffers retaining replaced surfaces are intentionally outside this metric.
  std::printf("rml_filter_memory frame=%llu active_bytes=%llu pooled_bytes=%llu owned_bytes=%llu sampled_peak_bytes=%llu pool_count=%zu creates=%llu reuses=%llu allocation_info=%u\n",
      static_cast<unsigned long long>(r.frames),static_cast<unsigned long long>(active),
      static_cast<unsigned long long>(pooled),static_cast<unsigned long long>(active+pooled),
      static_cast<unsigned long long>(r.filter_allocation_peak),r.filter_surface_pool.size(),
      static_cast<unsigned long long>(r.filter_surface_creates),static_cast<unsigned long long>(r.filter_surface_reuses),available?1u:0u);
}

rhi::ITextureView* layer_target(RmlRenderer& r, uint32_t layer) {
  if (layer == 0) return r.layers.empty() ? nullptr : r.layers[0].base_view;
  if (layer >= r.layers.size() || !r.layers[layer].texture) return nullptr;
  return r.layers[layer].target_view;
}

bool begin_surface_pass(RmlRenderer& r, RmlLayer& surface, bool clear_color) {
  if (!open_pass(r, surface.target_view.get(), clear_color || !surface.initialized,
                 surface.filter_stencil_view.get())) return false;
  r.target_origin_x=surface.origin_x;r.target_origin_y=surface.origin_y;
  r.target_width=surface.width;r.target_height=surface.height;
  surface.initialized = true;
  return true;
}

rhi::ITextureView* layer_source(RmlRenderer& r, uint32_t layer) {
  if (layer >= r.layers.size()) return nullptr;
  if (layer == 0) return r.layers[0].base_view;
  return r.layers[layer].sampled_view;
}

} // namespace rml_internal

} // namespace octaryn::client::rendering
