#pragma once

#include "JumpTransitions.h"
#include "WorldItemPose.h"
#include <span>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace octaryn::client::app {
namespace local_session { struct MeshCollisionSoup; }
struct MapManifest;

struct LocalPlayerInput {
  bool forward{}, backward{}, left{}, right{}, up{}, down{}, sprint{}, flying{};
 float yaw{}, pitch{};
 JumpTransitions jump_events;
 // Event-driven frames bypass the sampled up key for walking jumps.
 bool has_jump_events{};
};

struct LocalPlayerPose {
  float x{}, y{}, z{}, yaw{}, pitch{};
  float velocity_x{}, velocity_y{}, velocity_z{};
  bool on_ground{}, flying{};
  double source_seconds{};
  uint64_t source_tick{};
  float world_day_fraction{};
  double world_total_seconds{};
 bool jump_held{};
};

struct LocalMovementStats {
  uint64_t underruns{};
  double buffered_seconds{};
  bool holding{};
 uint64_t pending{}, ack{}, replays{}, corrections{}, overflows{};
 float correction_distance{}, max_correction_distance{};
};

class LocalSession {
public:
  LocalSession();
  ~LocalSession();
  LocalSession(const LocalSession&) = delete;
  LocalSession& operator=(const LocalSession&) = delete;

  bool start(const std::filesystem::path& client_bundle,
             const std::filesystem::path& world_root, uint32_t radius,
             const std::filesystem::path& log_root = {}, const MapManifest* map_override = nullptr);
  // Starts a remote session against a dedicated server endpoint ("host:port")
  // instead of spawning a packaged server. Mailbox files keep the same layout
  // so presentation, interpolation and acknowledgement behavior are unchanged.
  bool start_remote(const std::filesystem::path& client_bundle,
             const std::filesystem::path& world_root, uint32_t radius,
             const std::string& endpoint,
             const std::filesystem::path& log_root = {});
  void update(const LocalPlayerInput& input, double elapsed_seconds);
  void step_world_hours(int hours);
  // Publishes one module UI action (e.g. "inventory.drop", "interact.use")
  // to the authority through the ui_action intent mailbox.
  bool publish_ui_action(const std::string& action_id);
  bool poll_module_event(uint64_t& id, uint64_t& kind, uint64_t& p1, uint64_t& p2);
  bool publish_scene_physics(std::string request);
  bool scene_physics_snapshot(std::string& snapshot) const;
  // Immutable until the next update/stop. Source ticks use the 60 Hz authority clock.
  std::span<const WorldItemPose> world_items() const;
  uint64_t world_items_revision() const;
  // Client-side prediction collision source. The soup must be owned by the
  // caller and outlive the session (session start resets internal state, so
  // arm it after start/start_remote returns).
  void set_collision_mesh(const local_session::MeshCollisionSoup& soup);
  void warm_collision();
  void stop();
  bool running() const;
  bool player_pose(LocalPlayerPose& pose) const;
  // Latest received authority state, without prediction/interpolation/view overrides.
  bool authority_pose(LocalPlayerPose& pose, std::uint64_t& acknowledged_input) const;
  bool collision_ready(float x,float y,float z) const;
  std::uint64_t last_sent_input_frame() const;
  LocalMovementStats movement_stats() const;
  void set_radius(uint32_t radius);
  void set_benchmark_stream_center(int32_t x,int32_t z);
  const std::string& status() const;

  struct State;

 private:
  std::unique_ptr<State> state_;
};

} // namespace octaryn::client::app
