#pragma once
#include <slang-rhi.h>
#include "PlayerPose.h"

namespace octaryn::client::rendering {
struct WorldCamera;
struct PlayerRenderer;
PlayerRenderer* create_player_renderer(rhi::IDevice*, rhi::Format color_format,
    rhi::Format depth_format, const char* asset_path, const char* shader_path);
void destroy_player_renderer(PlayerRenderer*);
bool render_player(PlayerRenderer*, rhi::IRenderPassEncoder*, const WorldCamera&,
    int width, int height, const PlayerPose&, const PlayerLighting&, bool temporal=false, bool reset=false);
void commit_player_frame(PlayerRenderer*);
// Reuse a frame slot only after its submitted GPU fence completes.
bool prepare_player_shadows(PlayerRenderer*,rhi::ICommandEncoder*,unsigned slot,const PlayerPose&,bool ray_tracing);
bool bind_player_shadows(PlayerRenderer*,rhi::IShaderObject*,rhi::IAccelerationStructure* empty_scene);
bool render_player_shadow(PlayerRenderer*,rhi::IRenderPassEncoder*,const float* center,
    const float* right,const float* up,const float* forward,const float* projection);
}
