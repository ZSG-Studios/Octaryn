#pragma once
#include "AppClock.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include "WorldStartupReadiness.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace octaryn::client::app {
// Observes successful world presents; does not change loading/input policy.
class InitialPlayable {
  inline static std::uint64_t sessions_{};
  std::uint64_t session_{++sessions_};
  bool enabled_{};
  bool candidate_{},complete_{};
public:
  InitialPlayable() {
    const char* value=std::getenv("OCTARYN_CLIENT_STARTUP_READINESS");
    if(!value || !*value || std::strcmp(value,"0")==0)return;
    if(std::strcmp(value,"1")!=0)throw std::runtime_error("OCTARYN_CLIENT_STARTUP_READINESS requires 0 or 1");
    enabled_=true;
    std::puts("startup_readiness_profile enabled=1 clock_origin=main_entry cadence=every_successful_world_present cpu_scope=frame_total");
  }
  void presented(rendering::WorldRenderer* renderer,const rendering::WorldCamera& camera,
      std::uint64_t before_frame,const LocalSession& session) {
    if(!enabled_ || complete_)return;
    const auto stats=rendering::open_world_renderer_stats(renderer);
    if(stats.frames<=before_frame)return; // Minimized/null-acquire/menu work is not a world present.
    LocalPlayerPose actor;std::uint64_t ack{};
    if(!session.authority_pose(actor,ack) || !session.collision_ready(actor.x,actor.y,actor.z))return;
    const auto state=rendering::open_world_renderer_startup_readiness(renderer,camera);
    const auto& tiles=state.tiles;
    if(!tiles.total || !tiles.requested_ready || tiles.visible_missing)return;
    const auto elapsed=app_elapsed_ms();if(elapsed<0)return;
    const auto emit=[&](const char* marker) {
      std::printf("%s schema=1 clock_origin=main_entry elapsed_ms=%.3f renderer_frame=%llu session=%llu "
          "authority_ack=%llu requested_generation=%llu requested_set_hash=%llu requested_tiles=%u "
          "resident_tiles=%u total_tiles=%u visible_tiles=%u visible_missing=%u collision_ready=1 "
          "requested_ready=1 ray_required=%u ray_ready=%u ray_guard_complete=%u scope=%s "
          "actor_x=%.6f actor_y=%.6f actor_z=%.6f camera_x=%.6f camera_y=%.6f camera_z=%.6f "
          "yaw=%.6f pitch=%.6f fov=%.6f\n",marker,elapsed,
          static_cast<unsigned long long>(stats.frames-1),static_cast<unsigned long long>(session_),
          static_cast<unsigned long long>(ack),static_cast<unsigned long long>(tiles.generation),
          static_cast<unsigned long long>(tiles.requested_set_hash),tiles.requested,tiles.resident,tiles.total,
          tiles.visible,tiles.visible_missing,unsigned(state.ray_required),unsigned(state.ray_ready),
          unsigned(state.ray_guard_complete),state.ray_required?"all_manifest_rt_guard":"visible_requested_region",
          actor.x,actor.y,actor.z,camera.x,camera.y,camera.z,camera.yaw,camera.pitch,camera.vertical_fov);
    };
    if(!candidate_) {emit("world_initial_playable_candidate");candidate_=true;}
    if(state.ray_ready && state.ray_guard_complete) {emit("world_initial_playable");complete_=true;}
  }
};
}
