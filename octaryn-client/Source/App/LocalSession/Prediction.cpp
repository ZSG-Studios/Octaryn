#include "Prediction.h"
#include "MeshCollisionWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app::local_session {

namespace {

character_motion::State body_from_pose(const LocalPlayerPose& pose) {
  character_motion::State body{};
  body.x = pose.x;
  body.y = pose.y;
  body.z = pose.z;
  body.pitch = pose.pitch;
  body.yaw = pose.yaw;
  body.velocity_x = pose.velocity_x;
  body.velocity_y = pose.velocity_y;
  body.velocity_z = pose.velocity_z;
  body.is_on_ground = pose.on_ground ? 1u : 0u;
  body.control_mode = pose.flying ? 1u : 0u;
  body.jump_held = pose.jump_held ? 1u : 0u;
  return body;
}

LocalPlayerPose pose_from_body(const character_motion::State& body,
                               const LocalPlayerPose& authority) {
  LocalPlayerPose pose{};
  pose.x = body.x;
  pose.y = body.y;
  pose.z = body.z;
  pose.velocity_x = body.velocity_x;
  pose.velocity_y = body.velocity_y;
  pose.velocity_z = body.velocity_z;
  pose.on_ground = body.is_on_ground != 0;
  pose.flying = body.control_mode == 1;
  pose.jump_held = body.jump_held != 0;
  pose.world_day_fraction = authority.world_day_fraction;
  pose.world_total_seconds = authority.world_total_seconds;
  pose.source_seconds = authority.source_seconds;
  pose.source_tick = authority.source_tick;
  return pose;
}

} // namespace

void Prediction::set_collision(const MeshCollisionSoup& soup) {
  mesh_.positions = soup.positions.data();
  mesh_.position_count = soup.positions.size();
  mesh_.indices = soup.indices.data();
  mesh_.index_count = soup.indices.size();
}

void Prediction::warm_collision() {
  if (!collision_ready()) return;
  character_motion::acquire_mesh_world(mesh_);
}

void Prediction::simulate(character_motion::State& body,
                          const PredictionCommand& command) const {
  character_motion::Input input{};
  input.flags = command.flags;
  input.controller = 1;
  input.move_x = command.moveX;
  input.move_y = command.moveY;
  input.move_z = command.moveZ;
  input.camera_pitch = command.cameraPitch;
  input.camera_yaw = command.cameraYaw;
  input.relative_mouse = 1;
  character_motion::step_on_mesh(input, static_cast<float>(FixedDt), body, mesh_);
}

void Prediction::reconcile(const LocalPlayerPose& pose, uint64_t ack) {
  authority_ = pose;
  if (!initialized_) {
    yaw_ = pose.yaw;
    pitch_ = pose.pitch;
  }
  initialized_ = true;
  extrapolated_ = 0.0;

  if (!collision_ready() || ack == 0 || ack <= acknowledged_) {
    if (!collision_ready() || !body_seeded_) {
      body_ = body_from_pose(pose);
      body_seeded_ = true;
      error_x_ = error_y_ = error_z_ = 0.0f;
    }
    acknowledged_ = std::max(acknowledged_, ack);
    blocked_ = false;
    return;
  }

  // Retire the acknowledged commands, rewind to the authoritative state and
  // replay everything still outstanding.
  while (!pending_.empty() && pending_.front().frameIndex <= ack) {
    pending_.pop_front();
  }
  acknowledged_ = ack;

  const float shown_x = body_.x + error_x_;
  const float shown_y = body_.y + error_y_;
  const float shown_z = body_.z + error_z_;
  body_ = body_from_pose(pose);
  for (const auto& command : pending_) {
    simulate(body_, command);
    ++replays_;
  }
  error_x_ = shown_x - body_.x;
  error_y_ = shown_y - body_.y;
  error_z_ = shown_z - body_.z;
  const float error_squared = error_x_ * error_x_ + error_y_ * error_y_ +
                              error_z_ * error_z_;
  // Teleports and huge divergence snap; ordinary drift decays smoothly.
  if (error_squared > 2.5f * 2.5f) {
    error_x_ = error_y_ = error_z_ = 0.0f;
  }
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
  // Corrections decay toward the predicted body at a rate that hides float
  // drift within a few frames without ever fighting real movement.
  const float decay = std::exp(-static_cast<float>(std::min(elapsed, 0.25)) * 12.0f);
  error_x_ *= decay;
  error_y_ *= decay;
  error_z_ *= decay;
  extrapolated_ += elapsed;

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
    if (body_seeded_) {
      simulate(body_, command);
    }
  }
}

bool Prediction::sample(LocalPlayerPose& pose) const {
  if (!initialized_) return false;
  if (collision_ready() && body_seeded_) {
    pose = pose_from_body(body_, authority_);
    pose.x += error_x_;
    pose.y += error_y_;
    pose.z += error_z_;
    pose.yaw = yaw_;
    pose.pitch = pitch_;
    return true;
  }
  // Without collision data the session degrades to authoritative replay with
  // velocity dead-reckoning between snapshots.
  pose = authority_;
  const double seconds = std::min(extrapolated_, 0.25);
  pose.x += pose.velocity_x * static_cast<float>(seconds);
  pose.y += pose.velocity_y * static_cast<float>(seconds);
  pose.z += pose.velocity_z * static_cast<float>(seconds);
  pose.source_seconds += extrapolated_;
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
  result.replays = replays_;
  result.overflows = overflows_;
  return result;
}

}
