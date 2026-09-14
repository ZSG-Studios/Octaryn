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
}
