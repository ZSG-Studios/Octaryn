#pragma once
#include "LocalSession.h"
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

// Server-authoritative pose replay with velocity dead-reckoning between
// snapshots, plus presentation-local view angles and 60Hz command
// packetization toward the authoritative simulation. Local physics
// simulation returns with Box3D character motion.
class Prediction {
public:
 void reconcile(const LocalPlayerPose& pose, uint64_t ack);
 void advance(const LocalPlayerInput& input, double elapsed, double pose_age);
 bool sample(LocalPlayerPose& pose) const;
 bool sample_physics(LocalPlayerPose& pose) const { return sample(pose); }
 // Returns the unacknowledged history without consuming it: transport is
 // unreliable, so every packet re-sends all pending commands in order until
 // the authoritative acknowledgement retires them.
 PredictionPacket packet() const;
 LocalMovementStats stats() const;
private:
 static constexpr double FixedDt = 1.0 / 60.0;
 static constexpr size_t HistoryLimit = 128;

 LocalPlayerPose authority_{};
 double extrapolated_{};
 double accumulator_{};
 float yaw_{}, pitch_{};
 bool jump_observed_{}, jump_command_{};
 bool initialized_{}, blocked_{};
 uint64_t next_{}, acknowledged_{}, overflows_{};
 std::deque<PredictionCommand> pending_;
 std::deque<bool> jump_edges_;
};

}
