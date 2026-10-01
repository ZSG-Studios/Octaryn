#include "RmlRendererInternal.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
namespace {

constexpr int kMaxStops = 16;

void apply_color_stops(RmlShader& shader, const Rml::Dictionary& parameters) {
  const auto it = parameters.find("color_stop_list");
  if (it == parameters.end() || it->second.GetType() != Rml::Variant::COLORSTOPLIST) return;
  const auto& stops = it->second.GetReference<Rml::ColorStopList>();
  const int count = std::min(static_cast<int>(stops.size()), kMaxStops);
  shader.stop_positions.resize(count);
  shader.stop_colors.resize(count);
  for (int i = 0; i < count; ++i) {
    const Rml::ColorStop& stop = stops[static_cast<size_t>(i)];
    shader.stop_positions[static_cast<size_t>(i)] =
        stop.position.unit == Rml::Unit::NUMBER ? stop.position.number : 0.0f;
    // Stops arrive premultiplied; keep that convention for the gradient mix.
    const auto& color = stop.color;
    shader.stop_colors[static_cast<size_t>(i)] = {color.red / 255.0f, color.green / 255.0f,
                                                  color.blue / 255.0f, color.alpha / 255.0f};
  }
}

} // namespace

Rml::CompiledShaderHandle RmlRenderer::CompileShader(const Rml::String& name,
                                                     const Rml::Dictionary& parameters) {
  auto shader = std::make_shared<RmlShader>();

  if (name == "linear-gradient" || name == "radial-gradient" || name == "conic-gradient") {
    shader->type = RmlShaderType::Gradient;
    const bool repeating = Rml::Get(parameters, "repeating", false);
    if (name == "linear-gradient") {
      shader->gradient_function = repeating ? 3 : 0;
      shader->p_x = Rml::Get(parameters, "p0", Rml::Vector2f(0.0f)).x;
      shader->p_y = Rml::Get(parameters, "p0", Rml::Vector2f(0.0f)).y;
      const auto end = Rml::Get(parameters, "p1", Rml::Vector2f(0.0f));
      shader->v_x = end.x - shader->p_x;
      shader->v_y = end.y - shader->p_y;
    } else if (name == "radial-gradient") {
      shader->gradient_function = repeating ? 4 : 1;
      const auto center = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
      shader->p_x = center.x;
      shader->p_y = center.y;
      const auto radius = Rml::Get(parameters, "radius", Rml::Vector2f(1.0f));
      shader->v_x = radius.x != 0.0f ? 1.0f / radius.x : 0.0f;
      shader->v_y = radius.y != 0.0f ? 1.0f / radius.y : 0.0f;
    } else {
      shader->gradient_function = repeating ? 5 : 2;
      const auto center = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
      shader->p_x = center.x;
      shader->p_y = center.y;
      const float angle = Rml::Get(parameters, "angle", 0.0f);
      shader->v_x = std::cos(angle);
      shader->v_y = std::sin(angle);
    }
    apply_color_stops(*shader, parameters);
    if (shader->stop_positions.empty()) {
      Rml::Log::Message(Rml::Log::LT_WARNING, "Gradient '%s' has no color stops.", name.c_str());
      return 0;
    }
  } else if (name == "shader") {
    // Built-in procedural shader decorator; only "creation" is bundled.
    if (Rml::Get(parameters, "value", Rml::String()) != "creation") {
      Rml::Log::Message(Rml::Log::LT_WARNING, "Unsupported shader decorator value.");
      return 0;
    }
    shader->type = RmlShaderType::Creation;
    const auto dimensions = Rml::Get(parameters, "dimensions", Rml::Vector2f(0.0f));
    shader->dimensions_x = dimensions.x;
    shader->dimensions_y = dimensions.y;
  } else {
    Rml::Log::Message(Rml::Log::LT_WARNING, "Unsupported shader type '%s'.", name.c_str());
    return 0;
  }

  const auto handle = next_shader++;
  shaders.emplace(handle, std::move(shader));
  return handle;
}

void RmlRenderer::RenderShader(Rml::CompiledShaderHandle shader_handle,
                               Rml::CompiledGeometryHandle geometry_handle,
                               Rml::Vector2f translation, Rml::TextureHandle texture) {
  (void)texture;
  if (failed) return;
  const auto shader_it = shaders.find(shader_handle);
  const auto geometry_it = geometries.find(geometry_handle);
  if (shader_it == shaders.end() || geometry_it == geometries.end()) {
    failed = true;
    return;
  }
  const RmlShader& shader = *shader_it->second;

  std::array<float, 112> uniforms{};
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      uniforms[static_cast<size_t>(row * 4 + column)] = transform.GetRow(row)[column];
  uniforms[16] = translation.x;
  uniforms[17] = translation.y;
  uniforms[18] = 2.f / static_cast<float>(context_width);
  uniforms[19] = 2.f / static_cast<float>(context_height);

  rhi::IRenderPipeline* pipeline = nullptr;
  size_t size = 0;
  if (shader.type == RmlShaderType::Gradient) {
    uniforms[20] = shader.p_x;
    uniforms[21] = shader.p_y;
    uniforms[24] = shader.v_x;
    uniforms[25] = shader.v_y;
    uniforms[28] = static_cast<float>(shader.gradient_function);
    uniforms[29] = static_cast<float>(shader.stop_positions.size());
    for (size_t i = 0; i < shader.stop_colors.size(); ++i)
      for (size_t c = 0; c < 4; ++c)
        uniforms[32 + i * 4 + c] = shader.stop_colors[i][c];
    for (size_t i = 0; i < shader.stop_positions.size(); ++i)
      uniforms[96 + i] = shader.stop_positions[i];
    size = (96 + kMaxStops) * sizeof(float);
    pipeline = clip_mask_enabled ? gradient_clip.get() : gradient.get();
  } else {
    uniforms[20] = shader.creation_value;
    uniforms[21] = shader.dimensions_x;
    uniforms[22] = shader.dimensions_y;
    size = 24 * sizeof(float);
    pipeline = clip_mask_enabled ? creation_clip.get() : creation.get();
  }

  if (!rml_internal::draw_geometry(*this, pipeline, *geometry_it->second, uniforms.data(),
                                   size, nullptr)) {
    failed = true;
  }
}

void RmlRenderer::ReleaseShader(Rml::CompiledShaderHandle shader) { shaders.erase(shader); }

} // namespace octaryn::client::rendering
