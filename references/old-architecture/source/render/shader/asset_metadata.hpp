#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glaze/glaze.hpp>

struct shader_io_desc {
  std::string name{};
  std::string type{};
  std::uint32_t location{};
};

struct graphics_shader_metadata {
  std::uint32_t samplers{};
  std::uint32_t storage_textures{};
  std::uint32_t storage_buffers{};
  std::uint32_t uniform_buffers{};
  std::vector<shader_io_desc> inputs{};
  std::vector<shader_io_desc> outputs{};
};

struct compute_shader_metadata {
  std::uint32_t samplers{};
  std::uint32_t readonly_storage_textures{};
  std::uint32_t readonly_storage_buffers{};
  std::uint32_t readwrite_storage_textures{};
  std::uint32_t readwrite_storage_buffers{};
  std::uint32_t uniform_buffers{};
  std::uint32_t threadcount_x{};
  std::uint32_t threadcount_y{};
  std::uint32_t threadcount_z{};
};

namespace glz {

template <>
struct meta<shader_io_desc> {
  using T = shader_io_desc;
  static constexpr auto value = object("name", &T::name, "type", &T::type, "location", &T::location);
};

template <>
struct meta<graphics_shader_metadata> {
  using T = graphics_shader_metadata;
  static constexpr auto value = object("samplers", &T::samplers, "storage_textures", &T::storage_textures,
                                       "storage_buffers", &T::storage_buffers, "uniform_buffers",
                                       &T::uniform_buffers, "inputs", &T::inputs, "outputs", &T::outputs);
};

template <>
struct meta<compute_shader_metadata> {
  using T = compute_shader_metadata;
  static constexpr auto value = object(
    "samplers", &T::samplers, "readonly_storage_textures", &T::readonly_storage_textures,
    "readonly_storage_buffers", &T::readonly_storage_buffers, "readwrite_storage_textures",
    &T::readwrite_storage_textures, "readwrite_storage_buffers", &T::readwrite_storage_buffers,
    "uniform_buffers", &T::uniform_buffers, "threadcount_x", &T::threadcount_x, "threadcount_y",
    &T::threadcount_y, "threadcount_z", &T::threadcount_z);
};

}  // namespace glz
