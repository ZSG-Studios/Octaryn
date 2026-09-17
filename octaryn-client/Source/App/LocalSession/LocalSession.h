#pragma once

#include "BlockReceipts.h"
#include "JumpTransitions.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace octaryn::client::world_presentation { struct BlockEditIntent; }

namespace octaryn::client::app {

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
 uint16_t selected_block{};
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
             const std::filesystem::path& log_root = {});
  // Starts a remote session against a dedicated server endpoint ("host:port")
  // instead of spawning a packaged server. Mailbox files keep the same layout
  // so presentation, interpolation and acknowledgement behavior are unchanged.
  bool start_remote(const std::filesystem::path& client_bundle,
             const std::filesystem::path& world_root, uint32_t radius,
             const std::string& endpoint,
             const std::filesystem::path& log_root = {});
  void update(const LocalPlayerInput& input, double elapsed_seconds);
 using CollisionQuery = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t&);
 void set_collision_query(CollisionQuery query, void* context);
  bool submit_block_edit(const world_presentation::BlockEditIntent& edit, uint64_t* command_id = nullptr);
  const BlockReceipts& block_receipts() const;
 bool acknowledge_block_receipts(const std::string& session, uint64_t sequence);
 void step_world_hours(int hours);
  void stop();
  bool running() const;
  bool player_pose(LocalPlayerPose& pose) const;
  LocalMovementStats movement_stats() const;
  void set_radius(uint32_t radius);
  void set_benchmark_stream_center(int32_t x,int32_t z);
  const std::filesystem::path& chunk_stream_path() const;
  const std::string& status() const;

  struct State;

 private:
  std::unique_ptr<State> state_;
};

} // namespace octaryn::client::app
