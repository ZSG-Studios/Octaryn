#pragma once
#include "WorldRenderer.h"
#include "PlayerPose.h"
#include "LocalSession.h"
#include "Controls.h"
namespace octaryn::client::world_presentation { class WorldStream; class BlockInteraction; }
namespace octaryn::client::app {
rendering::WorldCamera player_camera(const LocalPlayerPose&, const WorldControls&, float fov,
    const world_presentation::WorldStream&, const world_presentation::BlockInteraction&);
// Map worlds have no voxel occluders; the boom stays at authored length.
rendering::WorldCamera player_camera_map(const LocalPlayerPose&, const WorldControls&, float fov);
rendering::PlayerPose player_presentation(const LocalPlayerPose&, const WorldControls&,
    const rendering::WorldCamera&, double presentation_seconds, double attack_until, uint64_t attack_sequence);
}
