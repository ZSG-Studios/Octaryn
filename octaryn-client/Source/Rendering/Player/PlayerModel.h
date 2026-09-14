#pragma once
#include <fastgltf/math.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::client::rendering {
constexpr size_t PlayerMaxJoints=64;
struct PlayerVertex {
  // Match Slang std430 explicitly; float3 members would otherwise add GPU padding.
  float position[4], normal[4], uv[4];
  uint32_t joints[4];
  float weights[4];
};
static_assert(sizeof(PlayerVertex)==80);
struct PlayerPrimitive {
  uint32_t first{}, count{};
  uint32_t first_person_first{}, first_person_count{};
  std::array<float,4> color{1,1,1,1};
  float metallic{}, roughness{1}, alpha_cutoff{};
};
struct PlayerNode {
  fastgltf::math::fvec3 translation{}, scale=fastgltf::math::fvec3(1.f);
  fastgltf::math::fquat rotation{0.f,0.f,0.f,1.f};
  int parent{-1};
  std::string name;
};
enum class PlayerTrackPath { Translation, Rotation, Scale };
struct PlayerTrack {
  uint32_t node{};
  PlayerTrackPath path{};
  bool step{};
  std::vector<float> times;
  std::vector<fastgltf::math::fvec4> values;
};
struct PlayerAnimation {
  std::string name;
  float duration{};
  bool looping{};
  std::vector<PlayerTrack> tracks;
};
struct PlayerModel {
  std::vector<PlayerVertex> vertices;
  std::vector<uint32_t> indices, joints;
  std::vector<uint32_t> first_person_indices;
  std::vector<PlayerPrimitive> primitives;
  std::vector<PlayerNode> nodes;
  std::vector<fastgltf::math::fmat4x4> inverse_bind;
  std::vector<PlayerAnimation> animations;
};
bool load_player_model(const std::filesystem::path&, PlayerModel&, std::string& error);
// Empty clip evaluates the authored bind pose. Unknown names fail explicitly.
bool sample_player_skin(const PlayerModel&, const std::string& clip, double seconds,
    std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>&);
struct PlayerLocalTransform {
  fastgltf::math::fvec3 translation{},scale=fastgltf::math::fvec3(1.f);
  fastgltf::math::fquat rotation{0.f,0.f,0.f,1.f};
};
struct PlayerLocalPose {
  std::array<PlayerLocalTransform,128> nodes;
  size_t count{};
};
bool sample_player_local_pose(const PlayerModel&,const std::string&,double,PlayerLocalPose&);
bool blend_player_local_pose(const PlayerLocalPose&,const PlayerLocalPose&,float,PlayerLocalPose&);
bool build_player_skin(const PlayerModel&,const PlayerLocalPose&,
    std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>&);
class PlayerAnimator {
public:
  bool sample(const PlayerModel&,const std::string& clip,uint64_t action_sequence,double source_seconds,
      std::array<fastgltf::math::fmat4x4,PlayerMaxJoints>&);
private:
  bool current_pose(const PlayerModel&,double source_seconds,PlayerLocalPose&) const;
  PlayerLocalPose blend_from_;
  std::string clip_;
  uint64_t sequence_{};
  double clip_start_{},blend_start_{},previous_time_{};
  bool initialized_{},blending_{};
};
}
