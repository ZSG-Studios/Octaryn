#pragma once
#include "WorldRenderer.h"
#include "LocalSession.h"
#include "Controls.h"
namespace octaryn::client::app {
// Map worlds have no voxel occluders; the boom stays at authored length.
rendering::WorldCamera player_camera_map(const LocalPlayerPose&, const WorldControls&, float fov);
}
