#include "PlayerModelData.h"

#include "Environment.h"
#include "Log.h"

#include <fastgltf/tools.hpp>

#include <algorithm>
#include <filesystem>

namespace octaryn_client_app {
namespace {

constexpr const char *kAssetRelativePath =
    "Client/Assets/Player/octaryn_player_v1.gltf";

player_node_pose pose_from_node(const fastgltf::Node &node) {
  if (const auto *trs = std::get_if<fastgltf::TRS>(&node.transform)) {
    return player_node_pose{trs->translation, trs->rotation, trs->scale};
  }
  return {};
}

void collect_parents(const fastgltf::Asset &asset,
                     std::vector<int32_t> &parents) {
  parents.assign(asset.nodes.size(), -1);
  for (size_t node_index = 0; node_index < asset.nodes.size(); ++node_index) {
    for (const size_t child : asset.nodes[node_index].children) {
      if (child < parents.size()) {
        parents[child] = static_cast<int32_t>(node_index);
      }
    }
  }
}

template <typename T>
std::vector<T> read_accessor(const fastgltf::Asset &asset, size_t index) {
  std::vector<T> values;
  const auto &accessor = asset.accessors[index];
  values.reserve(accessor.count);
  for (const T value : fastgltf::iterateAccessor<T>(asset, accessor)) {
    values.push_back(value);
  }
  return values;
}

bool load_mesh_vertices(const fastgltf::Asset &asset, player_model_asset &out) {
  if (asset.meshes.empty()) {
    return false;
  }
  const fastgltf::Mesh &mesh = asset.meshes[0];
  for (const fastgltf::Primitive &primitive : mesh.primitives) {
    const auto position_it = primitive.findAttribute("POSITION");
    const auto joint_it = primitive.findAttribute("JOINTS_0");
    const auto weight_it = primitive.findAttribute("WEIGHTS_0");
    if (position_it == primitive.attributes.end() ||
        joint_it == primitive.attributes.end() ||
        weight_it == primitive.attributes.end() ||
        !primitive.indicesAccessor.has_value()) {
      return false;
    }

    const std::vector<player_fvec3> positions =
        read_accessor<player_fvec3>(asset, position_it->accessorIndex);
    const std::vector<player_u16vec4> joints =
        read_accessor<player_u16vec4>(asset, joint_it->accessorIndex);
    const std::vector<player_fvec4> weights =
        read_accessor<player_fvec4>(asset, weight_it->accessorIndex);
    const uint32_t material = primitive.materialIndex.has_value()
                                  ? static_cast<uint32_t>(*primitive.materialIndex)
                                  : 0u;

    for (const uint16_t index :
         fastgltf::iterateAccessor<uint16_t>(
             asset, asset.accessors[*primitive.indicesAccessor])) {
      if (index >= positions.size() || index >= joints.size() ||
          index >= weights.size() ||
          out.vertices.size() >= kMaxPlayerModelVertices) {
        return false;
      }
      out.vertices.push_back(
          player_source_vertex{positions[index], joints[index], weights[index],
                               material});
    }
  }
  return !out.vertices.empty();
}

void load_skin(const fastgltf::Asset &asset, player_model_asset &out) {
  if (asset.skins.empty()) {
    return;
  }
  const fastgltf::Skin &skin = asset.skins[0];
  out.joints.assign(skin.joints.begin(), skin.joints.end());
  if (skin.inverseBindMatrices.has_value()) {
    out.inverse_bind =
        read_accessor<player_fmat4>(asset, *skin.inverseBindMatrices);
  }
}

void load_animations(const fastgltf::Asset &asset, player_model_asset &out) {
  for (const fastgltf::Animation &source : asset.animations) {
    player_animation_clip clip{};
    clip.name = std::string(source.name);
    for (const fastgltf::AnimationChannel &source_channel : source.channels) {
      if (!source_channel.nodeIndex.has_value() ||
          source_channel.samplerIndex >= source.samplers.size()) {
        continue;
      }
      const fastgltf::AnimationSampler &sampler =
          source.samplers[source_channel.samplerIndex];
      player_animation_channel channel{};
      channel.node = static_cast<uint32_t>(*source_channel.nodeIndex);
      channel.path = source_channel.path;
      channel.times = read_accessor<float>(asset, sampler.inputAccessor);
      if (channel.path == fastgltf::AnimationPath::Rotation) {
        channel.quat_values =
            read_accessor<player_fquat>(asset, sampler.outputAccessor);
      } else {
        channel.vec3_values =
            read_accessor<player_fvec3>(asset, sampler.outputAccessor);
      }
      if (!channel.times.empty()) {
        clip.duration = std::max(clip.duration, channel.times.back());
        clip.channels.push_back(std::move(channel));
      }
    }
    if (!clip.channels.empty()) {
      out.animations.push_back(std::move(clip));
    }
  }
}

} // namespace

bool load_player_model_asset(player_model_asset &out) {
  char path_buffer[4096] = {};
  if (!build_client_bundle_path(path_buffer, sizeof(path_buffer),
                                kAssetRelativePath,
                                "player_model_asset_path=failed")) {
    return false;
  }
  auto buffer = fastgltf::GltfDataBuffer::FromPath(path_buffer);
  if (!buffer) {
    log_line("player_model_asset=fastgltf_open_failed");
    return false;
  }

  fastgltf::Parser parser{};
  auto loaded = parser.loadGltf(
      buffer.get(), std::filesystem::path(path_buffer).parent_path(),
      fastgltf::Options::LoadExternalBuffers);
  if (!loaded) {
    log_line("player_model_asset=fastgltf_parse_failed");
    return false;
  }

  fastgltf::Asset asset = std::move(loaded.get());
  collect_parents(asset, out.parents);
  out.bind_pose.reserve(asset.nodes.size());
  for (const fastgltf::Node &node : asset.nodes) {
    out.bind_pose.push_back(pose_from_node(node));
  }
  load_skin(asset, out);
  load_animations(asset, out);
  out.loaded = load_mesh_vertices(asset, out) && !out.joints.empty() &&
               out.inverse_bind.size() == out.joints.size() &&
               !out.animations.empty();
  return out.loaded;
}

} // namespace octaryn_client_app
