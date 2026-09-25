#include "Prediction.h"

#include <algorithm>
#include <cmath>

namespace octaryn::client::app::local_session {

void Prediction::reconcile(const LocalPlayerPose& pose, uint64_t ack) {
  authority_ = pose;
  extrapolated_ = 0.0;
  if (!initialized_) {
    yaw_ = pose.yaw;
    pitch_ = pose.pitch;
  }
  initialized_ = true;
  // The server consumed every command up to the acknowledged frame; stop
  // resending them so the pending history stays bounded.
  while (!pending_.empty() && pending_.front().frameIndex <= ack) {
    pending_.pop_front();
  }
  acknowledged_ = ack;
  blocked_ = false;
}

void Prediction::advance(const LocalPlayerInput& input, double elapsed, double pose_age) {
  if (input.has_jump_events && input.jump_events.reset) {
    jump_edges_.clear();
    jump_command_ = false;
    jump_observed_ = false;
  }
  const auto observe_jump = [&](bool pressed) {
    if (pressed == jump_observed_) return;
    if (jump_edges_.size() < 32) {
      jump_edges_.push_back(pressed);
      jump_observed_ = pressed;
    } else {
      jump_edges_.clear();
      jump_command_ = false;
      jump_observed_ = pressed;
      ++overflows_;
    }
  };
  if (input.has_jump_events) {
    for (size_t index = 0;
         index < std::min(size_t(input.jump_events.count),
                          input.jump_events.pressed.size());
         ++index) {
      observe_jump(input.jump_events.pressed[index] && !input.flying);
    }
  } else {
    observe_jump(input.up && !input.flying);
  }

  // Look is presentation-local and never waits for a network acknowledgement.
  if (std::isfinite(input.yaw)) yaw_ = input.yaw;
  if (std::isfinite(input.pitch)) pitch_ = std::clamp(input.pitch, -1.55f, 1.55f);

  if (!initialized_) return;
  if (pose_age > 2.0) {
    // The authoritative stream stalled; keep the view responsive but stop
    // piling up commands the server may never consume.
    blocked_ = true;
    accumulator_ = 0.0;
    return;
  }
  if (pending_.size() >= HistoryLimit) {
    accumulator_ += std::min(elapsed, 8.0 / 60.0);
    blocked_ = true;
    ++overflows_;
    return;
  }

  accumulator_ += std::min(elapsed, 8.0 / 60.0);
  for (unsigned steps = 0; accumulator_ >= FixedDt && steps < 8 &&
                           pending_.size() < HistoryLimit;
       ++steps) {
    accumulator_ -= FixedDt;
    if (!jump_edges_.empty()) {
      jump_command_ = jump_edges_.front();
      jump_edges_.pop_front();
    }
    PredictionCommand command;
    command.frameIndex = ++next_;
    command.flags = (jump_command_ ? 1u : 0u) | (input.sprint ? 2u : 0u) |
                    (input.flying ? 4u : 0u);
    command.moveX = float(input.right) - float(input.left);
    command.moveY = input.flying ? float(input.up) - float(input.down) : 0.0f;
    command.moveZ = float(input.forward) - float(input.backward);
    command.cameraPitch = pitch_;
    command.cameraYaw = yaw_;
    pending_.push_back(command);
  }
}

bool Prediction::sample(LocalPlayerPose& pose) const {
  if (!initialized_) return false;
  pose = authority_;
  const double seconds = std::min(extrapolated_, 0.25);
  pose.x += pose.velocity_x * static_cast<float>(seconds);
  pose.y += pose.velocity_y * static_cast<float>(seconds);
  pose.z += pose.velocity_z * static_cast<float>(seconds);
  pose.source_seconds += extrapolated_;
  // View angles are the player's own; the authoritative echo never overrides
  // them locally.
  pose.yaw = yaw_;
  pose.pitch = pitch_;
  return true;
}

PredictionPacket Prediction::packet() const {
  PredictionPacket result;
  for (const auto& command : pending_) {
    if (result.commands.size() == 64) break;
    result.commands.push_back(command);
  }
  return result;
}

LocalMovementStats Prediction::stats() const {
  LocalMovementStats result;
  result.holding = blocked_;
  result.pending = static_cast<uint64_t>(pending_.size());
  result.ack = acknowledged_;
  result.overflows = overflows_;
  return result;
}

}
