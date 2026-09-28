#pragma once
#include "../../WorldStreaming/TileStartupReadiness.h"
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct WorldStartupReadiness {
  TileStartupReadiness tiles;
  bool ray_required{},ray_ready{},ray_guard_complete{};
};
WorldStartupReadiness open_world_renderer_startup_readiness(const WorldRenderer*,const WorldCamera&);
}
