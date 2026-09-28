#include "RmlRendererInternal.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
namespace {

template <size_t N>
void pack_blit(std::array<float, N>& uniforms, const RmlRenderer& r,
               const Rml::Rectanglei& dst, const Rml::Rectanglei& src,const RmlLayer& source) {
  static_assert(N >= 12);
  const float w = static_cast<float>(r.width);
  const float h = static_cast<float>(r.height);
  uniforms[0] = static_cast<float>(dst.Left());
  uniforms[1] = static_cast<float>(dst.Top());
  uniforms[2] = static_cast<float>(dst.Width());
  uniforms[3] = static_cast<float>(dst.Height());
  const auto uv=rml_internal::surface_uv(r,source,src);
  std::copy(uv.begin(),uv.end(),uniforms.begin()+4);
  uniforms[8] = w;
  uniforms[9] = h;
}

bool blit_pass(RmlRenderer& r, rhi::IRenderPipeline* pipeline, RmlLayer& target,
               rhi::ITextureView* source, const Rml::Rectanglei& dst,
               const float* uniforms, size_t size) {
  if (!rml_internal::begin_surface_pass(r, target, true)) return false;
  return rml_internal::blit(r, pipeline, source, dst, uniforms, size);
}

} // namespace

Rml::CompiledFilterHandle RmlRenderer::CompileFilter(const Rml::String& name,
                                                     const Rml::Dictionary& parameters) {
  auto filter = std::make_shared<RmlFilter>();
  auto matrix = [&]() -> std::array<float, 16>& { return filter->color_matrix; };
  auto diag = [](float r, float g, float b, float a) {
    return std::array<float, 16>{r, 0, 0, 0, 0, g, 0, 0, 0, 0, b, 0, 0, 0, 0, a};
  };

  if (name == "opacity") {
    filter->type = RmlFilterType::Passthrough;
    filter->blend_factor = Rml::Get(parameters, "value", 1.0f);
  } else if (name == "blur") {
    filter->type = RmlFilterType::Blur;
    filter->sigma = Rml::Get(parameters, "sigma", 1.0f);
  } else if (name == "drop-shadow") {
    filter->type = RmlFilterType::DropShadow;
    filter->sigma = Rml::Get(parameters, "sigma", 0.0f);
    const auto color = Rml::Get(parameters, "color", Rml::Colourb()).ToPremultiplied();
    filter->color[0] = color.red / 255.0f;
    filter->color[1] = color.green / 255.0f;
    filter->color[2] = color.blue / 255.0f;
    filter->color[3] = color.alpha / 255.0f;
    const auto offset = Rml::Get(parameters, "offset", Rml::Vector2f(0.0f));
    filter->offset_x = offset.x;
    filter->offset_y = offset.y;
  } else if (name == "brightness") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    matrix() = diag(value, value, value, 1.0f);
  } else if (name == "contrast") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    const float gray = 0.5f - 0.5f * value;
    matrix() = diag(value, value, value, 1.0f);
    matrix()[3] = gray; matrix()[7] = gray; matrix()[11] = gray; // constant column
  } else if (name == "invert") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = std::clamp(Rml::Get(parameters, "value", 1.0f), 0.0f, 1.0f);
    matrix() = diag(1.0f - 2.0f * value, 1.0f - 2.0f * value, 1.0f - 2.0f * value, 1.0f);
    matrix()[3] = value; matrix()[7] = value; matrix()[11] = value; // constant column
  } else if (name == "grayscale") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    const float rev = 1.0f - value;
    const float r = value * 0.2126f, g = value * 0.7152f, b = value * 0.0722f;
    matrix() = {r + rev, g, b, 0, r, g + rev, b, 0, r, g, b + rev, 0, 0, 0, 0, 1};
  } else if (name == "sepia") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    const float rev = 1.0f - value;
    const Rml::Vector3f rm = value * Rml::Vector3f(0.393f, 0.769f, 0.189f);
    const Rml::Vector3f gm = value * Rml::Vector3f(0.349f, 0.686f, 0.168f);
    const Rml::Vector3f bm = value * Rml::Vector3f(0.272f, 0.534f, 0.131f);
    matrix() = {rm.x + rev, rm.y, rm.z, 0, gm.x, gm.y + rev, gm.z, 0,
                bm.x, bm.y, bm.z + rev, 0, 0, 0, 0, 1};
  } else if (name == "hue-rotate") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    const float s = std::sin(value);
    const float c = std::cos(value);
    matrix() = {0.213f + 0.787f * c - 0.213f * s, 0.715f - 0.715f * c - 0.715f * s,
                0.072f - 0.072f * c + 0.928f * s, 0,
                0.213f - 0.213f * c + 0.143f * s, 0.715f + 0.285f * c + 0.140f * s,
                0.072f - 0.072f * c - 0.283f * s, 0,
                0.213f - 0.213f * c - 0.787f * s, 0.715f - 0.715f * c + 0.715f * s,
                0.072f + 0.928f * c + 0.072f * s, 0,
                0, 0, 0, 1};
  } else if (name == "saturate") {
    filter->type = RmlFilterType::ColorMatrix;
    const float value = Rml::Get(parameters, "value", 1.0f);
    matrix() = {0.213f + 0.787f * value, 0.715f - 0.715f * value, 0.072f - 0.072f * value, 0,
                0.213f - 0.213f * value, 0.715f + 0.285f * value, 0.072f - 0.072f * value, 0,
                0.213f - 0.213f * value, 0.715f - 0.715f * value, 0.072f + 0.928f * value, 0,
                0, 0, 0, 1};
  } else {
    Rml::Log::Message(Rml::Log::LT_WARNING, "Unsupported filter type '%s'.", name.c_str());
    return 0;
  }

  const auto handle = next_filter++;
  filters.emplace(handle, std::move(filter));
  return handle;
}

void RmlRenderer::ReleaseFilter(Rml::CompiledFilterHandle filter) { filters.erase(filter); }

namespace rml_internal {

bool apply_color_matrix(RmlRenderer& r, RmlLayer& source, RmlLayer& target,
                        const Rml::Rectanglei& region, const RmlFilter& filter) {
  std::array<float, 28> uniforms{};
  pack_blit(uniforms, r, region, region,source);
  for (int i = 0; i < 16; ++i) uniforms[12 + i] = filter.color_matrix[static_cast<size_t>(i)];
  return blit_pass(r, r.color_matrix.get(), target, source.sampled_view.get(), region,
                   uniforms.data(), uniforms.size() * sizeof(float));
}

// Two-pass separable gaussian: vertical into back, horizontal back into
// front, leaving the result in front for the next filter or composite.
bool apply_blur(RmlRenderer& r, RmlLayer& front, RmlLayer& back,
                const Rml::Rectanglei& region, const RmlFilter& filter) {
  ++r.blur_passes;
  const float w = static_cast<float>(r.width);
  const float h = static_cast<float>(r.height);

  for (int pass = 0; pass < 2; ++pass) {
    RmlLayer& target = pass == 0 ? back : front;
    RmlLayer& source = pass == 0 ? front : back;
    std::array<float, 20> uniforms{};
    pack_blit(uniforms, r, region, region,source);
    uniforms[12] = pass == 0 ? 0.0f : 1.0f / source.width; // texel offset
    uniforms[13] = pass == 0 ? 1.0f / source.height : 0.0f;
    const float scale = pass == 0 ? h / r.context_height : w / r.context_width;
    uniforms[14] = std::max(filter.sigma * scale, 0.0f);
    const auto uv=surface_uv(r,source,region);
    std::copy(uv.begin(),uv.end(),uniforms.begin()+16);
    if (!blit_pass(r, r.blur.get(), target, source.sampled_view.get(), region,
                   uniforms.data(), uniforms.size() * sizeof(float))) return false;
  }
  return true;
}

// Shadow silhouette into target, then the unmodified source over it.
bool apply_drop_shadow(RmlRenderer& r, RmlLayer& source, RmlLayer& target,
                       const Rml::Rectanglei& region, const RmlFilter& filter) {
  ++r.shadow_passes;
  std::array<float, 20> uniforms{};
  pack_blit(uniforms, r, region, region,source);
  const float w = static_cast<float>(r.width);
  const float h = static_cast<float>(r.height);
  uniforms[10] = filter.offset_x*w/r.context_width/source.width;
  uniforms[11] = filter.offset_y*h/r.context_height/source.height;
  const auto uv=surface_uv(r,source,region);
  std::copy(uv.begin(),uv.end(),uniforms.begin()+12);
  uniforms[16] = filter.color[0];
  uniforms[17] = filter.color[1];
  uniforms[18] = filter.color[2];
  uniforms[19] = filter.color[3];

  if (!rml_internal::begin_surface_pass(r, target, true)) return false;
  if (!rml_internal::blit(r, r.drop_shadow.get(), source.sampled_view.get(), region,
                          uniforms.data(), uniforms.size() * sizeof(float))) return false;

  if (filter.sigma > 0.0f) {
    const auto bounds=Rml::Rectanglei::FromPositionSize({target.origin_x,target.origin_y},{target.width,target.height});
    if (!ensure_surface(r, r.post_c, true,&bounds) || !apply_blur(r, target, r.post_c, region, filter))
      return false;
  }

  std::array<float, 12> composite{};
  pack_blit(composite, r, region, region,source);
  composite[10] = 1.0f;
  return rml_internal::blit(r, r.blit.get(), source.sampled_view.get(), region,
                            composite.data(), composite.size() * sizeof(float));
}

} // namespace rml_internal
} // namespace octaryn::client::rendering
