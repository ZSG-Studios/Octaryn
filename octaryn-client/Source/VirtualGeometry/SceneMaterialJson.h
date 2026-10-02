#pragma once
#include "../MapWorld/MapModel.h"
#include <glaze/glaze.hpp>

template<> struct glz::meta<octaryn::client::rendering::MapTexture> {
  using T=octaryn::client::rendering::MapTexture;
  static constexpr auto value=glz::object(
      "image",&T::image,"texcoord",&T::texcoord,"wrap_s",&T::wrap_s,"wrap_t",&T::wrap_t,
      "min_filter",&T::min_filter,"mag_filter",&T::mag_filter,"transform",&T::transform);
};
template<> struct glz::meta<octaryn::client::rendering::MapMaterial> {
  using T=octaryn::client::rendering::MapMaterial;
  static constexpr auto value=glz::object(
      "base_color",&T::base_color,"metallic",&T::metallic,"roughness",&T::roughness,
      "alpha_cutoff",&T::alpha_cutoff,"normal_scale",&T::normal_scale,"occlusion_strength",&T::occlusion_strength,
      "emissive",&T::emissive,"alpha_mode",&T::alpha_mode,"double_sided",&T::double_sided,"unlit",&T::unlit,
      "additive",&T::additive,"view_fade",&T::view_fade,"view_fade_parameters",&T::view_fade_parameters,
      "layer_count",&T::layer_count,"texture",&T::texture,"textures",&T::textures);
};
