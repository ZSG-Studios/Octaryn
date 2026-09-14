#pragma once
#include "PlayerRenderer.h"
#include "PlayerModel.h"
#include "WorldRenderer.h"

namespace octaryn::client::rendering {
struct PlayerRenderer {
  Slang::ComPtr<rhi::IDevice> device;
  PlayerModel model;
  Slang::ComPtr<rhi::IBuffer> vertices,indices;
  Slang::ComPtr<rhi::IRenderPipeline> pipeline,temporal_pipeline,shadow_pipeline;
  Slang::ComPtr<rhi::IComputePipeline> skin_pipeline;
  struct ShadowFrame {
    Slang::ComPtr<rhi::IBuffer> positions,instances,blas_scratch,tlas_scratch;
    Slang::ComPtr<rhi::IAccelerationStructure> blas,tlas;
  };
  std::array<ShadowFrame,2> shadows;
  unsigned shadow_slot{};
  bool shadow_visible{},skin_valid{};
  std::array<fastgltf::math::fmat4x4,PlayerMaxJoints> skin;
  PlayerPose skin_pose{};
  PlayerAnimator animation;
  std::array<fastgltf::math::fmat4x4,PlayerMaxJoints> previous_skin,pending_skin;
  PlayerPose previous_pose{},pending_pose{};
  WorldCamera previous_camera{},pending_camera{};
  int previous_width{},previous_height{},pending_width{},pending_height{};
  bool previous_valid{},pending_valid{};
};
bool sample_player_frame(PlayerRenderer&,const PlayerPose&);
}
