#include "PlayerModelAnimation.h"

#include "Log.h"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/memory/unique_ptr.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace octaryn_client_app {
namespace {

using ozz_animation_ptr = ozz::unique_ptr<ozz::animation::Animation>;
using ozz_skeleton_ptr = ozz::unique_ptr<ozz::animation::Skeleton>;

struct ozz_clip {
  std::string name;
  float duration = 1.0f;
  ozz_animation_ptr animation;
  ozz::animation::SamplingJob::Context context;
  std::vector<ozz::math::SoaTransform> locals;
};

ozz::math::Float3 to_float3(const player_fvec3 &v) {
  return ozz::math::Float3(v[0], v[1], v[2]);
}

ozz::math::Quaternion to_quat(const player_fquat &q) {
  return ozz::math::NormalizeSafe(ozz::math::Quaternion(q[0], q[1], q[2], q[3]),
                                  ozz::math::Quaternion::identity());
}

ozz::math::Float4x4 to_ozz_matrix(const player_fmat4 &m) {
  ozz::math::Float4x4 result{};
  result.cols[0] =
      ozz::math::simd_float4::Load(m[0][0], m[0][1], m[0][2], m[0][3]);
  result.cols[1] =
      ozz::math::simd_float4::Load(m[1][0], m[1][1], m[1][2], m[1][3]);
  result.cols[2] =
      ozz::math::simd_float4::Load(m[2][0], m[2][1], m[2][2], m[2][3]);
  result.cols[3] =
      ozz::math::simd_float4::Load(m[3][0], m[3][1], m[3][2], m[3][3]);
  return result;
}

void append_joint(const player_model_asset &asset, uint32_t node,
                  ozz::animation::offline::RawSkeleton::Joint::Children &out) {
  ozz::animation::offline::RawSkeleton::Joint joint{};
  const std::string name = std::to_string(node);
  joint.name = name.c_str();
  const player_node_pose &pose = asset.bind_pose[node];
  joint.transform.translation = to_float3(pose.translation);
  joint.transform.rotation = to_quat(pose.rotation);
  joint.transform.scale = to_float3(pose.scale);

  for (const uint32_t candidate : asset.joints) {
    if (candidate < asset.parents.size() &&
        asset.parents[candidate] == static_cast<int32_t>(node)) {
      append_joint(asset, candidate, joint.children);
    }
  }
  out.push_back(std::move(joint));
}

int joint_track_index(const player_model_asset &asset, uint32_t node) {
  const auto it = std::find(asset.joints.begin(), asset.joints.end(), node);
  if (it == asset.joints.end()) {
    return -1;
  }
  return static_cast<int>(it - asset.joints.begin());
}

void append_default_keys(const player_model_asset &asset, float duration,
                         ozz::animation::offline::RawAnimation &raw) {
  for (size_t joint_index = 0; joint_index < asset.joints.size();
       ++joint_index) {
    const player_node_pose &pose = asset.bind_pose[asset.joints[joint_index]];
    auto &track = raw.tracks[joint_index];
    track.translations.push_back({0.0f, to_float3(pose.translation)});
    track.rotations.push_back({0.0f, to_quat(pose.rotation)});
    track.scales.push_back({0.0f, to_float3(pose.scale)});
    if (duration > 0.0f) {
      track.translations.push_back({duration, to_float3(pose.translation)});
      track.rotations.push_back({duration, to_quat(pose.rotation)});
      track.scales.push_back({duration, to_float3(pose.scale)});
    }
  }
}

template <typename Keys, typename Value>
void replace_keys(Keys &keys, const std::vector<float> &times,
                  const std::vector<Value> &values) {
  keys.clear();
  const size_t count = std::min(times.size(), values.size());
  keys.reserve(count);
  float previous = -1.0f;
  for (size_t index = 0; index < count; ++index) {
    const float time = std::max(previous + 0.0001f, times[index]);
    keys.push_back({time, values[index]});
    previous = time;
  }
}

bool build_raw_skeleton(const player_model_asset &asset,
                        ozz_skeleton_ptr &skeleton) {
  ozz::animation::offline::RawSkeleton raw{};
  if (asset.joints.empty()) {
    return false;
  }
  append_joint(asset, asset.joints.front(), raw.roots);
  if (!raw.Validate() ||
      raw.num_joints() != static_cast<int>(asset.joints.size())) {
    return false;
  }
  skeleton = ozz::animation::offline::SkeletonBuilder()(raw);
  return skeleton != nullptr;
}

bool build_clip(const player_model_asset &asset,
                const player_animation_clip &clip,
                const ozz_skeleton_ptr &skeleton, ozz_clip &out) {
  ozz::animation::offline::RawAnimation raw{};
  raw.name = clip.name.c_str();
  raw.duration = std::max(clip.duration, 0.0001f);
  raw.tracks.resize(asset.joints.size());
  append_default_keys(asset, raw.duration, raw);

  for (const player_animation_channel &channel : clip.channels) {
    const int track_index = joint_track_index(asset, channel.node);
    if (track_index < 0) {
      continue;
    }
    auto &track = raw.tracks[static_cast<size_t>(track_index)];
    if (channel.path == fastgltf::AnimationPath::Rotation) {
      std::vector<ozz::math::Quaternion> values;
      values.reserve(channel.quat_values.size());
      for (const player_fquat &value : channel.quat_values) {
        values.push_back(to_quat(value));
      }
      replace_keys(track.rotations, channel.times, values);
    } else {
      std::vector<ozz::math::Float3> values;
      values.reserve(channel.vec3_values.size());
      for (const player_fvec3 &value : channel.vec3_values) {
        values.push_back(to_float3(value));
      }
      if (channel.path == fastgltf::AnimationPath::Translation) {
        replace_keys(track.translations, channel.times, values);
      } else if (channel.path == fastgltf::AnimationPath::Scale) {
        replace_keys(track.scales, channel.times, values);
      }
    }
  }

  if (!raw.Validate()) {
    return false;
  }
  out.name = clip.name;
  out.duration = raw.duration;
  out.animation = ozz::animation::offline::AnimationBuilder()(raw);
  if (out.animation == nullptr) {
    return false;
  }
  out.context.Resize(skeleton->num_joints());
  out.locals.resize(static_cast<size_t>(skeleton->num_soa_joints()));
  return true;
}

ozz_clip *find_clip(player_model_animation_runtime &runtime, const char *name);

} // namespace

struct player_model_animation_runtime {
  ozz_skeleton_ptr skeleton;
  std::vector<std::unique_ptr<ozz_clip>> clips;
  std::vector<ozz::math::SoaTransform> transition_locals;
  std::vector<ozz::math::Float4x4> inverse_bind;
  std::vector<ozz::math::SoaTransform> blended_locals;
  std::vector<ozz::math::Float4x4> model_matrices;
};

player_model_asset::player_model_asset() = default;
player_model_asset::~player_model_asset() = default;

namespace {

ozz_clip *find_clip(player_model_animation_runtime &runtime, const char *name) {
  for (const std::unique_ptr<ozz_clip> &clip : runtime.clips) {
    if (clip->name == name) {
      return clip.get();
    }
  }
  return runtime.clips.empty() ? nullptr : runtime.clips.front().get();
}

bool sample_clip(const player_model_animation_runtime &runtime, ozz_clip &clip,
                 float seconds) {
  const float ratio = clip.duration > 0.0f
                          ? std::fmod(seconds, clip.duration) / clip.duration
                          : 0.0f;
  ozz::animation::SamplingJob job{};
  job.animation = clip.animation.get();
  job.context = &clip.context;
  job.ratio = ratio;
  job.output = ozz::span<ozz::math::SoaTransform>(clip.locals.data(),
                                                  clip.locals.size());
  (void)runtime;
  return job.Run();
}

bool build_skin_from_locals(player_model_animation_runtime &runtime,
                            std::vector<ozz::math::Float4x4> &skin) {
  ozz::animation::LocalToModelJob local_to_model{};
  local_to_model.skeleton = runtime.skeleton.get();
  local_to_model.input = ozz::span<const ozz::math::SoaTransform>(
      runtime.blended_locals.data(), runtime.blended_locals.size());
  local_to_model.output = ozz::span<ozz::math::Float4x4>(
      runtime.model_matrices.data(), runtime.model_matrices.size());
  if (!local_to_model.Run()) {
    return false;
  }

  skin.clear();
  skin.reserve(runtime.inverse_bind.size());
  for (size_t joint = 0; joint < runtime.inverse_bind.size(); ++joint) {
    skin.push_back(runtime.model_matrices[joint] * runtime.inverse_bind[joint]);
  }
  return !skin.empty();
}

} // namespace

bool initialize_player_model_animation(player_model_asset &asset) {
  auto runtime = std::make_unique<player_model_animation_runtime>();
  if (!build_raw_skeleton(asset, runtime->skeleton)) {
    log_line("player_model_animation=ozz_skeleton_failed");
    return false;
  }

  runtime->inverse_bind.reserve(asset.inverse_bind.size());
  for (const player_fmat4 &matrix : asset.inverse_bind) {
    runtime->inverse_bind.push_back(to_ozz_matrix(matrix));
  }
  for (const player_animation_clip &clip : asset.animations) {
    auto built = std::make_unique<ozz_clip>();
    if (!build_clip(asset, clip, runtime->skeleton, *built)) {
      log_line("player_model_animation=ozz_clip_failed");
      return false;
    }
    runtime->clips.push_back(std::move(built));
  }
  runtime->blended_locals.resize(
      static_cast<size_t>(runtime->skeleton->num_soa_joints()));
  runtime->transition_locals.resize(
      static_cast<size_t>(runtime->skeleton->num_soa_joints()));
  runtime->model_matrices.resize(
      static_cast<size_t>(runtime->skeleton->num_joints()));
  asset.animation_runtime = std::move(runtime);
  return true;
}

void release_player_model_animation(player_model_asset &asset) {
  asset.animation_runtime.reset();
}

bool sample_player_model_animation(player_model_asset &asset,
                                   const char *animation_name, float seconds,
                                   std::vector<ozz::math::Float4x4> &skin) {
  if (asset.animation_runtime == nullptr) {
    return false;
  }
  player_model_animation_runtime &runtime = *asset.animation_runtime;
  ozz_clip *selected = find_clip(runtime, animation_name);
  if (selected == nullptr || !sample_clip(runtime, *selected, seconds)) {
    return false;
  }

  ozz_clip *idle = find_clip(runtime, "idle_loop");
  if (idle != nullptr && idle != selected &&
      sample_clip(runtime, *idle, seconds)) {
    ozz::animation::BlendingJob::Layer layers[2]{};
    layers[0].weight = 0.25f;
    layers[0].transform = ozz::span<const ozz::math::SoaTransform>(
        idle->locals.data(), idle->locals.size());
    layers[1].weight = 1.0f;
    layers[1].transform = ozz::span<const ozz::math::SoaTransform>(
        selected->locals.data(), selected->locals.size());
    ozz::animation::BlendingJob blend{};
    blend.layers =
        ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2u);
    blend.rest_pose = runtime.skeleton->joint_rest_poses();
    blend.output = ozz::span<ozz::math::SoaTransform>(
        runtime.blended_locals.data(), runtime.blended_locals.size());
    if (!blend.Run()) {
      return false;
    }
  } else {
    runtime.blended_locals = selected->locals;
  }

  return build_skin_from_locals(runtime, skin);
}

bool sample_player_model_animation_transition(
    player_model_asset &asset, const char *animation_name, float seconds,
    const char *previous_animation_name, float previous_seconds,
    float previous_weight, std::vector<ozz::math::Float4x4> &skin) {
  if (asset.animation_runtime == nullptr) {
    return false;
  }
  if (previous_weight <= 0.0f || previous_animation_name == nullptr ||
      std::strcmp(animation_name, previous_animation_name) == 0) {
    return sample_player_model_animation(asset, animation_name, seconds, skin);
  }
  player_model_animation_runtime &runtime = *asset.animation_runtime;
  ozz_clip *selected = find_clip(runtime, animation_name);
  ozz_clip *previous = find_clip(runtime, previous_animation_name);
  if (selected == nullptr || previous == nullptr ||
      !sample_clip(runtime, *selected, seconds) ||
      !sample_clip(runtime, *previous, previous_seconds)) {
    return false;
  }
  runtime.transition_locals = previous->locals;
  ozz::animation::BlendingJob::Layer layers[2]{};
  layers[0].weight = std::clamp(previous_weight, 0.0f, 1.0f);
  layers[0].transform = ozz::span<const ozz::math::SoaTransform>(
      runtime.transition_locals.data(), runtime.transition_locals.size());
  layers[1].weight = 1.0f - layers[0].weight;
  layers[1].transform = ozz::span<const ozz::math::SoaTransform>(
      selected->locals.data(), selected->locals.size());
  ozz::animation::BlendingJob blend{};
  blend.layers =
      ozz::span<const ozz::animation::BlendingJob::Layer>(layers, 2u);
  blend.rest_pose = runtime.skeleton->joint_rest_poses();
  blend.output = ozz::span<ozz::math::SoaTransform>(
      runtime.blended_locals.data(), runtime.blended_locals.size());
  if (!blend.Run()) {
    return false;
  }
  return build_skin_from_locals(runtime, skin);
}

} // namespace octaryn_client_app
