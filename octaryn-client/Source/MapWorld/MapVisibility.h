#pragma once
#include "Camera.h"
namespace octaryn::client::rendering {
struct WorldCamera;
struct WorldRenderer;
::camera map_visibility_camera(const WorldCamera&,const WorldRenderer&);
}
