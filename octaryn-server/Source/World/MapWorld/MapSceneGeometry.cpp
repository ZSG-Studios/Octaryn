#include "MapSceneGeometry.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <cmath>
#include <cstdio>
#include <numeric>
#include <system_error>
#include <type_traits>
#include <utility>

namespace octaryn::server::map_world {
namespace {

constexpr unsigned long long MaxGlbFileBytes = 512ull * 1024ull * 1024ull;
constexpr std::size_t MaxTriangles = 30ull * 1000ull * 1000ull;

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
                      MapTriangleSoup &soup) {
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
      position_accessor.type != fastgltf::AccessorType::Vec3) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=position_format\n");
    return false;
  }

  std::vector<uint32_t> primitive_indices;
  if (primitive.indicesAccessor.has_value()) {
    const fastgltf::Accessor &index_accessor =
        asset.accessors[*primitive.indicesAccessor];
    if (index_accessor.type != fastgltf::AccessorType::Scalar) {
      std::fprintf(stderr,
                   "server_live_map_world_load failed reason=index_format\n");
      return false;
    }
    primitive_indices.reserve(index_accessor.count);
    fastgltf::iterateAccessor<uint32_t>(
        asset, index_accessor,
        [&primitive_indices](uint32_t index) {
          primitive_indices.push_back(index);
        });
  } else {
    primitive_indices.resize(position_accessor.count);
    std::iota(primitive_indices.begin(), primitive_indices.end(), 0u);
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

  if (soup.triangle_count() + triangles.size() / 3u > MaxTriangles) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=triangle_cap triangles=%zu\n",
                 soup.triangle_count());
    return false;
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
      });

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
                 std::vector<char> &visited, bool &ok) {
  if (!ok || node_index >= asset.nodes.size() || visited[node_index] != 0u) {
    return;
  }
  visited[node_index] = 1;
  const fastgltf::Node &node = asset.nodes[node_index];
  const Mat4 world = multiply(parent, node_transform(node));
  if (node.meshIndex.has_value()) {
    const fastgltf::Mesh &mesh = asset.meshes[*node.meshIndex];
    for (const fastgltf::Primitive &primitive : mesh.primitives) {
      if (!append_primitive(asset, primitive, world, soup)) {
        ok = false;
        return;
      }
    }
  }
  for (const std::size_t child : node.children) {
    append_node(asset, child, world, soup, visited, ok);
    if (!ok) {
      return;
    }
  }
}

} // namespace

bool load_map_triangle_soup(const std::filesystem::path &glb_path,
                            MapTriangleSoup &soup) {
  std::error_code size_error;
  const auto file_bytes = std::filesystem::file_size(glb_path, size_error);
  if (size_error || file_bytes == 0u || file_bytes > MaxGlbFileBytes) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=file_size bytes=%llu\n",
                 static_cast<unsigned long long>(file_bytes));
    return false;
  }

  auto data = fastgltf::GltfDataBuffer::FromPath(glb_path);
  if (data.error() != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_read error=%u\n",
                 static_cast<unsigned>(data.error()));
    return false;
  }

  fastgltf::Parser parser;
  auto asset = parser.loadGltf(data.get(), glb_path.parent_path(),
                               fastgltf::Options::None);
  if (asset.error() != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_parse error=%u\n",
                 static_cast<unsigned>(asset.error()));
    return false;
  }
  if (fastgltf::validate(asset.get()) != fastgltf::Error::None) {
    std::fprintf(stderr,
                 "server_live_map_world_load failed reason=glb_validate\n");
    return false;
  }

  fastgltf::Asset &loaded = asset.get();
  std::vector<char> visited(loaded.nodes.size(), 0);
  bool ok = true;
  if (loaded.defaultScene.has_value() &&
      *loaded.defaultScene < loaded.scenes.size()) {
    for (const std::size_t node_index :
         loaded.scenes[*loaded.defaultScene].nodeIndices) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok);
    }
  } else if (!loaded.scenes.empty()) {
    for (const std::size_t node_index : loaded.scenes.front().nodeIndices) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok);
    }
  } else {
    for (std::size_t node_index = 0; node_index < loaded.nodes.size();
         ++node_index) {
      append_node(loaded, node_index, identity_matrix(), soup, visited, ok);
    }
  }
  return ok;
}

} // namespace octaryn::server::map_world
