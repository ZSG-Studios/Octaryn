#include "RmlRendererInternal.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
namespace {

// Current scissor in target pixels as an Rml rectangle.
Rml::Rectanglei current_region(const RmlRenderer& r) {
  const auto rect = rml_internal::scissor_pixels(r);
  return Rml::Rectanglei::FromPositionSize(
      {static_cast<int>(rect.minX), static_cast<int>(rect.minY)},
      {static_cast<int>(rect.maxX - rect.minX), static_cast<int>(rect.maxY - rect.minY)});
}

// dst_rect, src_uv, extra for a blit from the region of one frame-sized
// surface into the same pixel region of the current target.
std::array<float, 12> blit_uniforms(const RmlRenderer& r, const Rml::Rectanglei& dst,
                                    const Rml::Rectanglei& src, float color_scale, const RmlLayer* source=nullptr) {
  const float w = static_cast<float>(r.width);
  const float h = static_cast<float>(r.height);
  const auto uv=source?rml_internal::surface_uv(r,*source,src):
      std::array<float,4>{float(src.Left())/w,float(src.Top())/h,float(src.Right())/w,float(src.Bottom())/h};
  return {static_cast<float>(dst.Left()), static_cast<float>(dst.Top()),
          static_cast<float>(dst.Width()), static_cast<float>(dst.Height()),
          uv[0],uv[1],uv[2],uv[3],
          w, h, color_scale, 0.0f};
}

} // namespace

Rml::LayerHandle RmlRenderer::PushLayer() {
  if (failed) return 0;
  const uint32_t index = stack_depth + 1;
  if (index >= layers.size()) layers.resize(index + 1);
  if (!rml_internal::ensure_surface(*this, layers[index], true)) {
    failed = true;
    return 0;
  }
  ++layer_pushes;
  stack_depth = index;
  // The layer starts transparent black across its whole extent; composite
  // reads stay inside the scissor region.
  if (!rml_internal::begin_layer_pass(*this, index, true)) {
    failed = true;
    return 0;
  }
  return static_cast<Rml::LayerHandle>(index);
}

void RmlRenderer::PopLayer() {
  if (failed) return;
  if (stack_depth == 0) {
    failed = true;
    return;
  }
  --stack_depth;
  if (!rml_internal::begin_layer_pass(*this, stack_depth, false)) failed = true;
}

void RmlRenderer::CompositeLayers(Rml::LayerHandle source_handle,
                                  Rml::LayerHandle destination_handle,
                                  Rml::BlendMode blend_mode,
                                  Rml::Span<const Rml::CompiledFilterHandle> filter_handles) {
  if (failed) return;
  const uint32_t source = static_cast<uint32_t>(source_handle);
  const uint32_t destination = static_cast<uint32_t>(destination_handle);
  if (source >= layers.size() || !rml_internal::layer_source(*this, source) ||
      destination > stack_depth) {
    failed = true;
    return;
  }

  const Rml::Rectanglei region = current_region(*this);
  const bool replace = blend_mode == Rml::BlendMode::Replace;

  if (filter_handles.empty() && source != destination) {
    if (!rml_internal::begin_layer_pass(*this, destination, false)) { failed = true; return; }
    auto* pipeline = replace
        ? (clip_mask_enabled ? blit_replace_clip.get() : blit_replace.get())
        : (clip_mask_enabled ? blit_clip.get() : blit.get());
    const auto uniforms = blit_uniforms(*this, region, region, 1.0f);
    if (!rml_internal::blit(*this, pipeline, rml_internal::layer_source(*this, source),
                            region, uniforms.data(), uniforms.size() * sizeof(float))) {
      failed = true;
    }
    if (!failed && !rml_internal::begin_layer_pass(*this, stack_depth, false)) failed = true;
    return;
  }

  // Filtered path: copy the source region into post_a, apply filters
  // ping-pong between the post surfaces, then composite onto the destination.
  int halo_x=1,halo_y=1;
  for(auto handle:filter_handles) {
    const auto found=filters.find(handle);if(found==filters.end()){failed=true;return;}
    const auto& filter=*found->second;
    if(filter.type==RmlFilterType::Blur || filter.type==RmlFilterType::DropShadow) {
      halo_x+=int(std::ceil((3*filter.sigma+std::abs(filter.offset_x))*width/context_width));
      halo_y+=int(std::ceil((3*filter.sigma+std::abs(filter.offset_y))*height/context_height));
    }
  }
  const auto bounds=Rml::Rectanglei::FromCorners(
      {std::max(0,region.Left()-halo_x),std::max(0,region.Top()-halo_y)},
      {std::min(width,region.Right()+halo_x),std::min(height,region.Bottom()+halo_y)});
  if (!rml_internal::ensure_surface(*this, post_a, true,&bounds) ||
      !rml_internal::ensure_surface(*this, post_b, true,&bounds)) {
    failed = true;
    return;
  }

  if (!rml_internal::begin_surface_pass(*this, post_a, true)) { failed = true; return; }
  {
    const auto uniforms = blit_uniforms(*this, region, region, 1.0f);
    if (!rml_internal::blit(*this, blit_replace.get(),
                            rml_internal::layer_source(*this, source), region,
                            uniforms.data(), uniforms.size() * sizeof(float))) {
      failed = true;
      return;
    }
  }

  RmlLayer* front = &post_a;
  RmlLayer* back = &post_b;
  for (const Rml::CompiledFilterHandle handle : filter_handles) {
    const auto it = filters.find(handle);
    if (it == filters.end()) { failed = true; return; }
    const RmlFilter& filter = *it->second;
    ++filter_passes;
    switch (filter.type) {
      case RmlFilterType::Passthrough: {
        const auto uniforms = blit_uniforms(*this, region, region, filter.blend_factor,front);
        if (!rml_internal::begin_surface_pass(*this, *back, true) ||
            !rml_internal::blit(*this, blit_replace.get(), front->sampled_view.get(), region,
                                uniforms.data(), uniforms.size() * sizeof(float))) {
          failed = true;
          return;
        }
        std::swap(front, back);
        break;
      }
      case RmlFilterType::MaskImage: {
        ++mask_passes;
        std::array<float, 16> uniforms{};
        const auto base = blit_uniforms(*this, region, region, 1.0f,front);
        std::copy(base.begin(), base.end(), uniforms.begin());
        // The mask remains frame-sized while filtered source UVs are cropped.
        const auto mask_uv=rml_internal::surface_uv(*this,mask_layer,region);
        std::copy(mask_uv.begin(),mask_uv.end(),uniforms.begin()+12);
        if (!rml_internal::begin_surface_pass(*this, *back, true) ||
            !rml_internal::blit(*this, blend_mask.get(), front->sampled_view.get(), region,
                                uniforms.data(), uniforms.size() * sizeof(float),
                                mask_layer.sampled_view.get())) {
          failed = true;
          return;
        }
        std::swap(front, back);
        break;
      }
      case RmlFilterType::ColorMatrix:
        if (!rml_internal::apply_color_matrix(*this, *front, *back, region, filter)) { failed = true; return; }
        std::swap(front, back);
        break;
      case RmlFilterType::Blur:
        if (!rml_internal::apply_blur(*this, *front, *back, region, filter)) { failed = true; return; }
        break; // blur leaves the result in front
      case RmlFilterType::DropShadow:
        if (!rml_internal::apply_drop_shadow(*this, *front, *back, region, filter)) { failed = true; return; }
        std::swap(front, back);
        break;
      default:
        failed = true;
        return;
    }
  }

  if (!rml_internal::begin_layer_pass(*this, destination, false)) { failed = true; return; }
  auto* pipeline = replace ? (clip_mask_enabled ? blit_replace_clip.get() : blit_replace.get())
                           : (clip_mask_enabled ? blit_clip.get() : blit.get());
  const auto uniforms = blit_uniforms(*this, region, region, 1.0f,front);
  if (!rml_internal::blit(*this, pipeline, front->sampled_view.get(), region,
                          uniforms.data(), uniforms.size() * sizeof(float))) {
    failed = true;
  }
  if (!failed && !rml_internal::begin_layer_pass(*this, stack_depth, false)) failed = true;
}

Rml::TextureHandle RmlRenderer::SaveLayerAsTexture() {
  if (failed || !clipped || stack_depth == 0) return 0;
  const Rml::Rectanglei bounds = current_region(*this);
  if (bounds.Width() <= 0 || bounds.Height() <= 0) return 0;
  const RmlLayer& top = layers[stack_depth];
  if (!top.texture) return 0; // The base layer cannot be a copy source.

  auto texture = std::make_shared<RmlTexture>();
  rhi::TextureDesc desc{};
  desc.size = {static_cast<uint32_t>(bounds.Width()), static_cast<uint32_t>(bounds.Height()), 1};
  desc.format = format;
  desc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::CopyDestination;
  desc.defaultState = rhi::ResourceState::CopyDestination;
  if (SLANG_FAILED(device->createTexture(desc, nullptr, texture->image.writeRef())) ||
      SLANG_FAILED(texture->image->getDefaultView(texture->view.writeRef()))) {
    failed = true;
    return 0;
  }
  rml_internal::end_pass(*this);
  commands->copyTexture(texture->image.get(), {0, 1, 0, 1}, {},
                        top.texture.get(), {0, 1, 0, 1},
                        {static_cast<uint32_t>(bounds.Left()), static_cast<uint32_t>(bounds.Top()), 0},
                        {static_cast<uint32_t>(bounds.Width()),
                         static_cast<uint32_t>(bounds.Height()), 1});
  texture->width = static_cast<uint32_t>(bounds.Width());
  texture->height = static_cast<uint32_t>(bounds.Height());
  const auto handle = next_texture++;
  textures.emplace(handle, std::move(texture));
  texture_peak = std::max(texture_peak, textures.size());
  return handle;
}

Rml::CompiledFilterHandle RmlRenderer::SaveLayerAsMaskImage() {
  if (failed || stack_depth == 0) return 0;
  if (!rml_internal::ensure_surface(*this, mask_layer, true)) {
    failed = true;
    return 0;
  }
  const Rml::Rectanglei region = current_region(*this);
  if (!rml_internal::begin_surface_pass(*this, mask_layer, true)) {
    failed = true;
    return 0;
  }
  const auto uniforms = blit_uniforms(*this, region, region, 1.0f);
  if (!rml_internal::blit(*this, blit_replace.get(),
                          rml_internal::layer_source(*this, stack_depth), region,
                          uniforms.data(), uniforms.size() * sizeof(float))) {
    failed = true;
    return 0;
  }
  // Resume rendering into the previous layer.
  if (!rml_internal::begin_layer_pass(*this, stack_depth, false)) {
    failed = true;
    return 0;
  }
  auto filter = std::make_shared<RmlFilter>();
  filter->type = RmlFilterType::MaskImage;
  const auto handle = next_filter++;
  filters.emplace(handle, std::move(filter));
  return handle;
}

} // namespace octaryn::client::rendering
