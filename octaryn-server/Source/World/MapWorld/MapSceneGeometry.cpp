#include "MapSceneGeometry.h"
#include "FilePath.h"
#include "MapSceneLimits.h"
#include "GltfBufferViews.h"
#include "GltfCollisionImport.h"
#include "GltfSourceRange.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <system_error>
#include <type_traits>
#include <utility>
#include <exception>

namespace octaryn::server::map_world {
namespace {
// Row-major 4x4 helper for flattening the node hierarchy; glTF stays +Y up.
struct Mat4 {
  float m[4][4];
};

Mat4 identity_matrix() {
  Mat4 matrix{};
  matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = matrix.m[3][3] = 1.0f;
  return matrix;
}

Mat4 multiply(const Mat4 &parent, const Mat4 &child) {
  Mat4 result{};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      float sum = 0.0f;
      for (int inner = 0; inner < 4; ++inner) {
        sum += parent.m[row][inner] * child.m[inner][column];
      }
      result.m[row][column] = sum;
    }
  }
  return result;
}

Mat4 trs_matrix(const fastgltf::TRS &trs) {
  const fastgltf::math::fquat &rotation = trs.rotation;
  const float x = rotation.x();
  const float y = rotation.y();
  const float z = rotation.z();
  const float w = rotation.w();
  const float xx = 2.0f * x * x;
  const float yy = 2.0f * y * y;
  const float zz = 2.0f * z * z;
  const float xy = 2.0f * x * y;
  const float xz = 2.0f * x * z;
  const float yz = 2.0f * y * z;
  const float wx = 2.0f * w * x;
  const float wy = 2.0f * w * y;
  const float wz = 2.0f * w * z;

  Mat4 matrix = identity_matrix();
  matrix.m[0][0] = 1.0f - yy - zz;
  matrix.m[0][1] = xy - wz;
  matrix.m[0][2] = xz + wy;
  matrix.m[1][0] = xy + wz;
  matrix.m[1][1] = 1.0f - xx - zz;
  matrix.m[1][2] = yz - wx;
  matrix.m[2][0] = xz - wy;
  matrix.m[2][1] = yz + wx;
  matrix.m[2][2] = 1.0f - xx - yy;
  for (int row = 0; row < 3; ++row) {
    matrix.m[row][0] *= trs.scale.x();
    matrix.m[row][1] *= trs.scale.y();
    matrix.m[row][2] *= trs.scale.z();
  }
  matrix.m[0][3] = trs.translation.x();
  matrix.m[1][3] = trs.translation.y();
  matrix.m[2][3] = trs.translation.z();
  return matrix;
}

Mat4 node_transform(const fastgltf::Node &node) {
  return std::visit(
      [](const auto &value) -> Mat4 {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, fastgltf::TRS>) {
          return trs_matrix(value);
        } else {
          Mat4 matrix{};
          for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
              matrix.m[row][column] = value.col(
                  static_cast<std::size_t>(column))[static_cast<std::size_t>(row)];
            }
          }
          return matrix;
        }
      },
      node.transform);
}

void transform_point(const Mat4 &matrix, const float position[3],
                     float out[3]) {
  for (int row = 0; row < 3; ++row) {
    out[row] = matrix.m[row][0] * position[0] +
               matrix.m[row][1] * position[1] +
               matrix.m[row][2] * position[2] + matrix.m[row][3];
  }
}

bool append_primitive(const fastgltf::Asset &asset,
                      const fastgltf::Primitive &primitive, const Mat4 &world,
                      MapTriangleSoup &soup,octaryn::assets::GltfBufferViews& buffers) {
  buffers.clear();
  if (!primitive.targets.empty()) {
    std::fprintf(stderr, "server_live_map_world_load failed reason=morph_targets_unsupported\n");
    return false;
  }
  const auto &m = world.m;
  const float determinant =
      m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
      m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
      m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  if (!std::isfinite(determinant) || determinant == 0.0f) {
    std::fprintf(stderr, "server_live_map_world_load failed reason=singular_transform\n");
    return false;
  }
  if (primitive.type != fastgltf::PrimitiveType::Triangles &&
      primitive.type != fastgltf::PrimitiveType::TriangleStrip &&
      primitive.type != fastgltf::PrimitiveType::TriangleFan) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=primitive_type type=%u\n",
                 static_cast<unsigned>(primitive.type));
    return false;
  }

  const auto position_attribute = primitive.findAttribute("POSITION");
  if (position_attribute == primitive.attributes.end()) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=missing_position\n");
    return false;
  }
  const fastgltf::Accessor &position_accessor =
      asset.accessors[position_attribute->accessorIndex];
  if (position_accessor.componentType != fastgltf::ComponentType::Float ||
      position_accessor.type != fastgltf::AccessorType::Vec3 ||
      position_accessor.count > soup.max_triangles * 3u - soup.positions.size() / 3u) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=position_format\n");
    return false;
  }

  std::vector<uint32_t> primitive_indices;
  if (primitive.indicesAccessor.has_value()) {
    const fastgltf::Accessor &index_accessor =
        asset.accessors[*primitive.indicesAccessor];
    if (index_accessor.type != fastgltf::AccessorType::Scalar ||
        index_accessor.count > soup.max_triangles * 3u) {
      std::fprintf(stderr,
                   "server_live_map_world_load failed reason=index_format\n");
      return false;
    }
    primitive_indices.reserve(index_accessor.count);
    fastgltf::iterateAccessor<uint32_t>(
        asset, index_accessor,
        [&primitive_indices](uint32_t index) {
          primitive_indices.push_back(index);
        },buffers);
  } else {
    primitive_indices.resize(position_accessor.count);
    std::iota(primitive_indices.begin(), primitive_indices.end(), 0u);
  }

  const std::size_t count = primitive_indices.size();
  const bool triangle_list = primitive.type == fastgltf::PrimitiveType::Triangles;
  if (count < 3u || (triangle_list && count % 3u != 0u) ||
      (triangle_list ? count / 3u : count - 2u) > soup.max_triangles - soup.triangle_count()) {
    std::fprintf(stderr, "server_live_map_world_load failed reason=triangle_count\n");
    return false;
  }
  std::vector<uint32_t> triangles;
  const auto collect_triangle = [&triangles](uint32_t a, uint32_t b,
                                             uint32_t c) {
    triangles.insert(triangles.end(), {a, b, c});
  };
  if (primitive.type == fastgltf::PrimitiveType::Triangles) {
    triangles = std::move(primitive_indices);
  } else if (primitive.type == fastgltf::PrimitiveType::TriangleStrip) {
    for (std::size_t index = 2; index < primitive_indices.size(); ++index) {
      if (index % 2u == 0u) {
        collect_triangle(primitive_indices[index - 2],
                         primitive_indices[index - 1],
                         primitive_indices[index]);
      } else {
        collect_triangle(primitive_indices[index - 1],
                         primitive_indices[index - 2],
                         primitive_indices[index]);
      }
    }
  } else {
    for (std::size_t index = 2; index < primitive_indices.size(); ++index) {
      collect_triangle(primitive_indices[0], primitive_indices[index - 1],
                       primitive_indices[index]);
    }
  }

  if (soup.triangle_count() + triangles.size() / 3u > soup.max_triangles) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=triangle_cap triangles=%zu\n",
                 soup.triangle_count());
    return false;
  }

  if (determinant < 0.0f) {
    for (std::size_t index = 0; index < triangles.size(); index += 3u) {
      std::swap(triangles[index + 1u], triangles[index + 2u]);
    }
  }
  const std::size_t vertex_base = soup.positions.size() / 3u;
  const std::size_t vertex_count = position_accessor.count;
  bool all_finite = true;
  soup.positions.reserve(soup.positions.size() + vertex_count * 3u);
  fastgltf::iterateAccessor<fastgltf::math::fvec3>(
      asset, position_accessor,
      [&](const fastgltf::math::fvec3 &position) {
        const float local[3] = {position.x(), position.y(), position.z()};
        float world_position[3];
        transform_point(world, local, world_position);
        for (const float component : world_position) {
          soup.positions.push_back(component);
          if (!std::isfinite(component)) {
            all_finite = false;
          }
        }
      },buffers);

  if (!all_finite) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=non_finite_position\n");
    return false;
  }
  for (const uint32_t index : triangles) {
    if (index >= vertex_count) {
      std::fprintf(stderr,
                   "server_live_map_world_load failed reason=index_range\n");
      return false;
    }
    soup.indices.push_back(static_cast<uint32_t>(vertex_base + index));
  }
  return true;
}

void append_node(const fastgltf::Asset &asset, std::size_t node_index,
                 const Mat4 &parent, MapTriangleSoup &soup,
                 std::vector<char> &visited, bool &ok,octaryn::assets::GltfBufferViews& buffers,
                 const octaryn::assets::GltfCollisionImport& collision) {
  if (!ok || node_index >= asset.nodes.size() || visited[node_index] != 0u) {
    return;
  }
  visited[node_index] = 1;
  const fastgltf::Node &node = asset.nodes[node_index];
  if (node.skinIndex.has_value()) {
    ok = false;
    return;
  }
  const Mat4 world = multiply(parent, node_transform(node));
  if (node.meshIndex.has_value() && collision.enabled(*node.meshIndex) &&
      !std::binary_search(soup.excluded_nodes.begin(),soup.excluded_nodes.end(),std::string(node.name))) {
    const fastgltf::Mesh &mesh = asset.meshes[*node.meshIndex];
    for (const fastgltf::Primitive &primitive : mesh.primitives) {
      if (!append_primitive(asset, primitive, world, soup,buffers)) {
        ok = false;
        return;
      }
    }
  }
  for (const std::size_t child : node.children) {
    append_node(asset, child, world, soup, visited, ok,buffers,collision);
    if (!ok) {
      return;
    }
  }
}

} // namespace

bool load_map_triangle_soup(const std::filesystem::path &glb_path,
                            MapTriangleSoup &soup) {
  try {
  std::error_code size_error;
  const auto file_bytes = std::filesystem::file_size(content::file_io_path(glb_path), size_error);
  if (size_error || file_bytes == 0u || (!soup.source_length && (soup.source_offset || file_bytes > soup.max_file_bytes))) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=file_size bytes=%llu\n",
                 static_cast<unsigned long long>(file_bytes));
    return false;
  }

  auto data = [&] {
    if(soup.source_length) {
      const auto bytes=assets::read_gltf_source_range(glb_path,soup.source_offset,soup.source_length,soup.max_file_bytes);
      return fastgltf::GltfDataBuffer::FromBytes(bytes.data(),bytes.size());
    }
    return fastgltf::GltfDataBuffer::FromPath(content::file_io_path(glb_path));
  }();
  if (data.error() != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_read error=%u\n",
                 static_cast<unsigned>(data.error()));
    return false;
  }

  fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform |
                         fastgltf::Extensions::KHR_materials_emissive_strength |
                         fastgltf::Extensions::EXT_meshopt_compression);
  octaryn::assets::GltfCollisionImport collision;collision.bind(parser);
  auto asset = parser.loadGltf(data.get(), glb_path.parent_path(),fastgltf::Options::None);
  if (asset.error() != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_parse error=%u\n",
                 static_cast<unsigned>(asset.error()));
    return false;
  }
  octaryn::assets::validate_gltf_accessors(asset.get());
  collision.validate();
  if (fastgltf::validate(asset.get()) != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_validate\n");
    return false;
  }

  fastgltf::Asset &loaded = asset.get();
  if(!map_scene_fits(loaded,soup)) {
    std::fprintf(stderr,"server_live_map_world_load failed reason=scene_exceeds_collision_preparation_limits\n");
    return false;
  }
  octaryn::assets::GltfBufferViews buffers(glb_path.parent_path(),std::size_t(soup.max_file_bytes),nullptr);
  std::vector<char> visited(loaded.nodes.size(), 0);
  bool ok = true;
  if (loaded.defaultScene.has_value() &&
      *loaded.defaultScene < loaded.scenes.size()) {
    for (const std::size_t node_index :
         loaded.scenes[*loaded.defaultScene].nodeIndices) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok,buffers,collision);
    }
  } else if (!loaded.scenes.empty()) {
    for (const std::size_t node_index : loaded.scenes.front().nodeIndices) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok,buffers,collision);
    }
  } else {
    for (std::size_t node_index = 0; node_index < loaded.nodes.size();
         ++node_index) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok,buffers,collision);
    }
  }
  return ok;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"server_live_map_world_load failed reason=buffer_decode detail=%s\n",error.what());
    return false;
  }
}

} // namespace octaryn::server::map_world
