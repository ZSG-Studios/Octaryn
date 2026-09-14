#pragma once
#include "LocalSession.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace octaryn::client::app::local_session {
class PoseHistory {
public:
  bool push(const LocalPlayerPose& pose) {
    if (!history_.empty()) {
      const auto& previous = history_.back();
      if (pose.source_tick <= previous.source_tick || pose.source_seconds <= previous.source_seconds) return false;
      const float dx = pose.x - previous.x, dy = pose.y - previous.y, dz = pose.z - previous.z;
      if (dx * dx + dy * dy + dz * dz > 1024.0f || pose.source_seconds - previous.source_seconds > 0.5) {
        history_.clear();
        playing_ = false;
      }
    }
    if (history_.empty()) cursor_ = pose.source_seconds;
    history_.push_back(pose);
    while (history_.size() > 64) history_.pop_front();
    return true;
  }

  void advance(double elapsed) {
    if (history_.empty() || !std::isfinite(elapsed)) return;
    if (!playing_ && history_.back().source_seconds - cursor_ >= 0.05) playing_ = true;
    if (playing_) {
      cursor_ += std::max(0.0, elapsed);
      const double newest = history_.back().source_seconds;
      if (cursor_ > newest) {
        // Reaching a valid endpoint is not an outage. The next snapshot can arrive
        // before the next render; only a real overrun requires the refill delay.
        const double tolerance = 4 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(newest));
        if (cursor_ - newest > tolerance) { playing_ = false; ++underruns_; }
        cursor_ = newest;
      }
    }
    while (history_.size() > 2 && history_[1].source_seconds <= cursor_) history_.pop_front();
  }

  bool sample(LocalPlayerPose& result) const {
    if (history_.empty()) return false;
    result = history_.front();
    if (history_.size() == 1 || cursor_ <= result.source_seconds) return true;
    const LocalPlayerPose* a = &history_.front();
    const LocalPlayerPose* b = a;
    for (const auto& pose : history_) {
      b = &pose;
      if (pose.source_seconds >= cursor_) break;
      a = &pose;
    }
    // Discrete state belongs to the earlier tick until the actual double-precision
    // boundary; a float interpolation weight can round to one before that time.
    result = cursor_ < b->source_seconds ? *a : *b;
    const double duration = b->source_seconds - a->source_seconds;
    if (duration <= 0.0) return true;
    const float t = static_cast<float>(std::clamp((cursor_ - a->source_seconds) / duration, 0.0, 1.0));
    const float velocity_dot = a->velocity_x * b->velocity_x + a->velocity_y * b->velocity_y + a->velocity_z * b->velocity_z;
    const bool smooth = a->on_ground == b->on_ground && a->flying == b->flying && velocity_dot > 0.001f;
    const auto coordinate = [=](float p0, float p1, float v0, float v1) {
      if (!smooth) return std::lerp(p0, p1, t);
      const float t2 = t * t, t3 = t2 * t, dt = static_cast<float>(duration);
      const float value = (2 * t3 - 3 * t2 + 1) * p0 + (t3 - 2 * t2 + t) * dt * v0
          + (-2 * t3 + 3 * t2) * p1 + (t3 - t2) * dt * v1;
      return std::clamp(value, std::min(p0, p1), std::max(p0, p1));
    };
    result.x = coordinate(a->x, b->x, a->velocity_x, b->velocity_x);
    result.y = coordinate(a->y, b->y, a->velocity_y, b->velocity_y);
    result.z = coordinate(a->z, b->z, a->velocity_z, b->velocity_z);
    result.velocity_x = std::lerp(a->velocity_x, b->velocity_x, t);
    result.velocity_y = std::lerp(a->velocity_y, b->velocity_y, t);
    result.velocity_z = std::lerp(a->velocity_z, b->velocity_z, t);
    result.pitch = std::lerp(a->pitch, b->pitch, t);
    result.yaw = a->yaw + std::remainder(b->yaw - a->yaw, 6.28318530718f) * t;
    result.source_seconds = cursor_;
    result.world_total_seconds = std::lerp(a->world_total_seconds, b->world_total_seconds, static_cast<double>(t));
    const float phase = a->world_day_fraction + std::remainder(b->world_day_fraction - a->world_day_fraction, 1.0f) * t;
    result.world_day_fraction = phase - std::floor(phase);
    return true;
  }

  const LocalPlayerPose& latest() const { return history_.back(); }
  bool empty() const { return history_.empty(); }
  LocalMovementStats stats() const {
    return {underruns_, history_.empty() ? 0 : std::max(0.0, history_.back().source_seconds - cursor_),
        !history_.empty() && !playing_};
  }
private:
  std::deque<LocalPlayerPose> history_;
  double cursor_{};
  bool playing_{};
  uint64_t underruns_{};
};
}
