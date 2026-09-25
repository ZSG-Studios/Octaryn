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
class Prediction {
public:
 using Query = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t&);
 void set_collision(Query query, void* context) { query_=query; context_=context; }
 void reconcile(const LocalPlayerPose& pose, uint64_t ack);
 void advance(const LocalPlayerInput& input, double elapsed, double pose_age);
 bool sample(LocalPlayerPose& pose) const;
 bool sample_physics(LocalPlayerPose& pose) const;
 PredictionPacket packet() const;
 LocalMovementStats stats() const;
private:
 static uint32_t query(void* self, int32_t x, int32_t y, int32_t z);
 bool simulate(const PredictionCommand& command);
 bool retry_collision();
 character_motion::State render_body() const;
 character_motion::State body_{}, previous_body_{};
 LocalPlayerPose authority_{};
 std::deque<PredictionCommand> pending_;
 std::deque<bool> jump_edges_;
 Query query_{};
 void* context_{};
 uint64_t next_{}, ack_{}, replays_{}, corrections_{}, overflows_{};
 double accumulator_{};
 float correction_x_{}, correction_y_{}, correction_z_{};
 float correction_distance_{}, max_correction_distance_{};
 bool initialized_{}, collision_ready_{}, missing_{}, collision_blocked_{}, blocked_{}, observed_jump_{}, command_jump_{};
};
}
