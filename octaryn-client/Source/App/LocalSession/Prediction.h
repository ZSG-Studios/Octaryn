#pragma once
#include "LocalSession.h"
#include "CharacterMotion.h"
#include <deque>
#include <vector>

namespace octaryn::client::app::local_session {

struct PredictionCommand {
 uint64_t frameIndex{};
 uint32_t flags{}, controller{1};
 float moveX{}, moveY{}, moveZ{}, cameraPitch{}, cameraYaw{};
 int relativeMouse{1};
};
struct PredictionPacket {
 int version{2};
 std::vector<PredictionCommand> commands;
};

// Owning copy of a renderer map soup, kept alive as the collision cache key.
struct MeshCollisionSoup {
  std::vector<float> positions;
  std::vector<std::uint32_t> indices;
};


// Full client-side prediction against the authoritative simulation: the same
// Box3D character motion the server runs steps locally at command cadence, so
// movement and jumping feel immediate. Each authoritative snapshot rewinds
// the body to the acknowledged state and replays the pending commands; the
// remaining correction is carried as a decaying render offset instead of a
// visible snap. View angles are always presentation-local.
class Prediction {
public:
 // The soup must outlive this object; the collision world caches on it.
 void set_collision(const MeshCollisionSoup& soup);
 bool collision_ready() const { return mesh_.positions != nullptr; }
 // Builds the Box3D collision world during a loading screen.
 void warm_collision();

 void reconcile(const LocalPlayerPose& pose, uint64_t ack);
 void advance(const LocalPlayerInput& input, double elapsed, double pose_age);
 bool sample(LocalPlayerPose& pose) const;
 bool sample_physics(LocalPlayerPose& pose) const { return sample(pose); }
 // Presentation-local view angles for the fallback replay path.
 void view(float& yaw, float& pitch) const { yaw = yaw_; pitch = pitch_; }
 // Returns the unacknowledged history without consuming it: transport is
 // unreliable, so every packet re-sends all pending commands in order until
 // the authoritative acknowledgement retires them.
 PredictionPacket packet() const;
 LocalMovementStats stats() const;
private:
 static constexpr double FixedDt = 1.0 / 60.0;
 static constexpr size_t HistoryLimit = 128;

 void simulate(character_motion::State& body, const PredictionCommand& command) const;

 LocalPlayerPose authority_{};
 character_motion::State body_{};
 double accumulator_{};
 double extrapolated_{};
 float yaw_{}, pitch_{};
 float error_x_{}, error_y_{}, error_z_{};
 bool jump_observed_{}, jump_command_{};
 bool initialized_{}, blocked_{};
 bool body_seeded_{};
 uint64_t next_{}, acknowledged_{}, overflows_{}, replays_{};
 character_motion::MeshCollision mesh_{};
 std::deque<PredictionCommand> pending_;
 std::deque<bool> jump_edges_;
};

}
