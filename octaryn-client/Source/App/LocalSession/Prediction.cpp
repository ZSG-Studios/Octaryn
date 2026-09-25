#include "Prediction.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::app::local_session {

void Prediction::reconcile(const LocalPlayerPose& pose, uint64_t ack) {
  (void)ack;
  authority_ = pose;
  extrapolated_ = 0.0;
  initialized_ = true;
}

void Prediction::advance(const LocalPlayerInput& input, double elapsed, double pose_age) {
  (void)input;
  (void)pose_age;
  if (!initialized_) return;
  extrapolated_ += elapsed;
  ++frame_;
}

bool Prediction::sample(LocalPlayerPose& pose) const {
  if (!initialized_) return false;
  pose = authority_;
  const double seconds = std::min(extrapolated_, 0.25);
  pose.x += pose.velocity_x * static_cast<float>(seconds);
  pose.y += pose.velocity_y * static_cast<float>(seconds);
  pose.z += pose.velocity_z * static_cast<float>(seconds);
  pose.source_seconds += extrapolated_;
  return true;
}

PredictionPacket Prediction::packet() const { return PredictionPacket{}; }

LocalMovementStats Prediction::stats() const { return LocalMovementStats{}; }

}
