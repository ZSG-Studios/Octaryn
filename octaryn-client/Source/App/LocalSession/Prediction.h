#pragma once
#include "LocalSession.h"
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
// snapshots. Local physics simulation returns with Box3D character motion;
// this keeps the session smooth without voxel collision data.
class Prediction {
public:
 using Query = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t&);
 void set_collision(Query, void*) {}
 void reconcile(const LocalPlayerPose& pose, uint64_t ack);
 void advance(const LocalPlayerInput& input, double elapsed, double pose_age);
 bool sample(LocalPlayerPose& pose) const;
 bool sample_physics(LocalPlayerPose& pose) const { return sample(pose); }
 PredictionPacket packet() const;
 LocalMovementStats stats() const;
private:
 LocalPlayerPose authority_{};
 double extrapolated_{};
 bool initialized_{};
 uint64_t frame_{};
};

}
