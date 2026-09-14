#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace octaryn::client::world_presentation { struct BlockEditIntent; }

namespace octaryn::client::app {

struct LocalPlayerInput {
  bool forward{}, backward{}, left{}, right{}, up{}, down{}, sprint{}, flying{};
  float yaw{}, pitch{};
};

struct LocalPlayerPose {
  float x{}, y{}, z{}, yaw{}, pitch{};
  float velocity_x{}, velocity_y{}, velocity_z{};
  bool on_ground{}, flying{};
  double source_seconds{};
  uint64_t source_tick{};
  float world_day_fraction{};
  double world_total_seconds{};
};

struct LocalMovementStats {
  uint64_t underruns{};
  double buffered_seconds{};
  bool holding{};
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
  void update(const LocalPlayerInput& input, double elapsed_seconds);
  bool submit_block_edit(const world_presentation::BlockEditIntent& edit);
  void stop();
  bool running() const;
  bool player_pose(LocalPlayerPose& pose) const;
  LocalMovementStats movement_stats() const;
  void set_radius(uint32_t radius);
  void set_benchmark_stream_center(int32_t x,int32_t z);
  const std::filesystem::path& chunk_stream_path() const;
  const std::string& status() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace octaryn::client::app
