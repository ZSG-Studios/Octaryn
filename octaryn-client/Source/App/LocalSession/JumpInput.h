#pragma once
#include <cstdint>
#include <deque>

namespace octaryn::client::app::local_session {
// Keep both edges stable across latest-value mailboxes until a server tick consumes them.
class JumpInput {
public:
 bool observe(bool pressed) {
 if (pressed == observed_) return true;
 if (edges_.size() == 32) return false;
 edges_.push_back({pressed, 0});
 observed_ = pressed;
 return true;
 }
 void acknowledge(uint64_t frame) {
 if (!edges_.empty() && edges_.front().frame && frame >= edges_.front().frame) {
 applied_ = edges_.front().pressed;
 edges_.pop_front();
 }
 }
 bool pressed() const { return edges_.empty() ? applied_ : edges_.front().pressed; }
 void published(uint64_t frame) {
 if (!edges_.empty() && !edges_.front().frame) edges_.front().frame = frame;
 }
private:
 struct Edge { bool pressed; uint64_t frame; };
 std::deque<Edge> edges_;
 bool observed_{}, applied_{};
};
}
